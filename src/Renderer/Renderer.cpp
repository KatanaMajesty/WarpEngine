#include "Renderer.h"

#include <cmath>
#include "../Util/String.h"
#include "../Core/Logger.h"
#include "../Core/Assert.h"
#include "../Core/Application.h"

// TODO: Temp, remove
#include "../Math/Math.h"
#include "../World/World.h"
#include "../World/Components.h"
#include "RHI/PIXRuntime.h"


namespace Warp
{

	// We want 16-byte boundary alignment for components
	// Previously we had 28-byte boundary here, which lead to issues
	struct alignas(16) HlslDirectionalLight
	{
		float Intensity;
		Math::Vector3 Direction;
		Math::Vector3 Radiance;
	};

	struct alignas(256) HlslLightEnvironment
	{
		static constexpr uint32_t MaxDirectionalLights = 3;
		static constexpr uint32_t MaxSphereLights = 0; // NOIMPL
		static constexpr uint32_t MaxSpotLights = 0; // NOIMPL

		uint32_t NumDirectionalLights = 0;
		HlslDirectionalLight DirectionalLights[MaxDirectionalLights];
	};

	struct alignas(256) HlslViewData
	{
		Math::Matrix ViewMatrix;
		Math::Matrix ViewInvMatrix;
		Math::Matrix ProjMatrix;
	};

	using EHlslDrawPropertyFlags = uint32_t;
	enum EHlslDrawPropertyFlag
	{
		eHlslDrawPropertyFlag_None = 0,
		eHlslDrawPropertyFlag_HasTexCoords = 1,
		eHlslDrawPropertyFlag_HasTangents = 2,
		eHlslDrawPropertyFlag_HasBitangents = 4,
		eHlslDrawPropertyFlag_NoNormalMap = 8,
		eHlslDrawPropertyFlag_NoRoughnessMetalnessMap = 16,
		eHlslDrawPropertyFlag_NoBaseColorMap = 32,
	};

	struct alignas(256) HlslDrawData
	{
		Math::Matrix InstanceToWorld;
		Math::Matrix NormalMatrix;
		EHlslDrawPropertyFlags DrawFlags;
	};

	// Represents indices of Basic.hlsl root signature
	enum BasicRootParameterIndex
	{
		BasicRootParameterIndex_CbLightEnv,
		BasicRootParameterIndex_CbViewData,
		BasicRootParameterIndex_CbDrawData,
		BasicRootParameterIndex_Positions,
		BasicRootParameterIndex_Normals,
		BasicRootParameterIndex_TexCoords,
		BasicRootParameterIndex_Tangents,
		BasicRootParameterIndex_Bitangents,
		BasicRootParameterIndex_Meshlets,
		BasicRootParameterIndex_UniqueVertexIndices,
		BasicRootParameterIndex_PrimitiveIndices,
		BasicRootParameterIndex_BaseColor,
		BasicRootParameterIndex_NormalMap,
		BasicRootParameterIndex_MetalnessRoughnessMap,
	};

	Renderer::Renderer(HWND hwnd)
		: m_physicalDevice(
			[hwnd]() -> std::unique_ptr<RHIPhysicalDevice>
			{
				RHIPhysicalDeviceDesc physicalDeviceDesc;
				physicalDeviceDesc.hwnd = hwnd;
#ifdef WARP_DEBUG
				physicalDeviceDesc.EnableDebugLayer = true;
				physicalDeviceDesc.EnableGpuBasedValidation = true;
#else
				physicalDeviceDesc.EnableDebugLayer = false;
				physicalDeviceDesc.EnableGpuBasedValidation = false;
#endif
				return std::make_unique<RHIPhysicalDevice>(physicalDeviceDesc);
			}())
		, m_device(std::make_unique<RHIDevice>(GetPhysicalDevice()))
		, m_graphicsContext(RHICommandContext(L"RHICommandContext_Graphics", m_device->GetGraphicsQueue()))
		, m_copyContext(RHICopyCommandContext(L"RHICommandContext_Copy", m_device->GetCopyQueue()))
		, m_computeContext(RHICommandContext(L"RHICommandContext_Compute", m_device->GetComputeQueue()))
		, m_swapchain(std::make_unique<RHISwapchain>(m_physicalDevice.get()))
	{
		WARP_ASSERT(m_device->CheckMeshShaderSupport(), "we only use mesh shaders");

		// TODO: Temp, remove
		if (!InitAssets())
		{
			WARP_LOG_ERROR("Failed to init assets");
			return;
		}

		InitDepthStencil();

		WARP_LOG_INFO("Renderer was initialized successfully");
	}
	
	Renderer::~Renderer()
	{
		WaitForGfxToFinish();
		m_computeContext.GetQueue()->HostWaitIdle();
		m_copyContext.GetQueue()->HostWaitIdle();
	}

	void Renderer::Resize(uint32_t width, uint32_t height)
	{
		// Wait for all frames to be completed before resizing the backbuffers
		// as they may still be used
		WaitForGfxToFinish();
		m_swapchain->Resize(width, height);

		ResizeDepthStencil();
	}

	void Renderer::Render(World* world)
	{
		// TODO: "Kind of" mesh instance impl
		struct MeshInstance
		{
			Math::Matrix InstanceToWorld;
			Math::Matrix NormalMatrix;
			EHlslDrawPropertyFlags DrawFlags;
			MeshAsset* Mesh;
		};

		std::vector<MeshInstance> meshInstances;
		world->GetEntityRegistry().view<MeshComponent, TransformComponent>().each(
			[&meshInstances](MeshComponent& meshComponent, const TransformComponent& transformComponent)
			{
				MeshInstance& instance = meshInstances.emplace_back();

				Math::Matrix translation = Math::Matrix::CreateTranslation(transformComponent.Translation);
				Math::Matrix rotation = Math::Matrix::CreateFromYawPitchRoll(transformComponent.Rotation);
				Math::Matrix scale = Math::Matrix::CreateScale(transformComponent.Scaling);

				MeshAsset* mesh = meshComponent.GetMesh();

				instance.InstanceToWorld = scale * rotation * translation;
				instance.InstanceToWorld.Invert(instance.NormalMatrix);
				instance.NormalMatrix.Transpose(instance.NormalMatrix);
				instance.Mesh = mesh;
				instance.DrawFlags = eHlslDrawPropertyFlag_None;

				if (mesh->HasAttributes(EVertexAttributes_TextureCoords))
					instance.DrawFlags |= eHlslDrawPropertyFlag_HasTexCoords;

				if (mesh->HasAttributes(EVertexAttributes_Tangents))
					instance.DrawFlags |= eHlslDrawPropertyFlag_HasTangents;

				if (mesh->HasAttributes(EVertexAttributes_Bitangents))
					instance.DrawFlags |= eHlslDrawPropertyFlag_HasBitangents;

				if (!mesh->Material.HasNormalMap() || 
					!mesh->Material.Manager->IsValid<TextureAsset>(mesh->Material.NormalMapProxy))
					instance.DrawFlags |= eHlslDrawPropertyFlag_NoNormalMap;

				if (!mesh->Material.HasMetalnessRoughnessMap() || 
					!mesh->Material.Manager->IsValid<TextureAsset>(mesh->Material.MetalnessRoughnessMapProxy))
					instance.DrawFlags |= eHlslDrawPropertyFlag_NoRoughnessMetalnessMap;

				if (!mesh->Material.HasBaseColor() ||
					!mesh->Material.Manager->IsValid<TextureAsset>(mesh->Material.BaseColorProxy))
					instance.DrawFlags |= eHlslDrawPropertyFlag_NoBaseColorMap;
			}
		);

		if (meshInstances.empty())
		{
			return;
		}

		HlslLightEnvironment environment = HlslLightEnvironment();
		for (auto&& [_, dirLightComponent] : world->GetEntityRegistry().view<DirectionalLightComponent>().each())
		{
			if (environment.NumDirectionalLights >= HlslLightEnvironment::MaxDirectionalLights)
			{
				WARP_ASSERT(false, "Too many dir lights! Handle this");
				break;
			}

			environment.DirectionalLights[environment.NumDirectionalLights++] = HlslDirectionalLight{
				.Intensity = dirLightComponent.Intensity,
				.Direction = dirLightComponent.Direction,
				.Radiance = dirLightComponent.Radiance,
			};
		}

		Entity worldCamera = world->GetWorldCamera();
		const EulersCameraComponent& cameraComponent = worldCamera.GetComponent<EulersCameraComponent>();

		RHIDevice* Device = m_device.get();

		UINT frameIndex = m_swapchain->GetCurrentBackbufferIndex();
		WaitForGfxOnFrameToFinish(frameIndex);

		Device->BeginFrame();
		m_graphicsContext.Open();
		{
			WARP_SCOPED_EVENT(&m_graphicsContext, fmt::format("Renderer_RenderWorld_Frame{}", frameIndex + 1));

			UINT Width = m_swapchain->GetWidth();
			UINT Height = m_swapchain->GetHeight();
			m_graphicsContext.SetViewport(0, 0, Width, Height);
			m_graphicsContext.SetScissorRect(0, 0, Width, Height);

			RHITexture* backbuffer = m_swapchain->GetBackbuffer(frameIndex);

			{
				WARP_SCOPED_EVENT(&m_graphicsContext, "Renderer_RenderWorld_SetDescriptorHeaps");

				std::array<ID3D12DescriptorHeap*, 2> descriptorHeaps = {
					Device->GetSamplerHeap()->GetD3D12Heap(),
					Device->GetViewHeap()->GetD3D12Heap()
				};

				m_graphicsContext->SetDescriptorHeaps(static_cast<UINT>(descriptorHeaps.size()), descriptorHeaps.data());
			}

			// Setting rtvs
			{
				WARP_SCOPED_EVENT(&m_graphicsContext, "Renderer_RenderWorld_SetRtvs");

				m_graphicsContext.AddTransitionBarrier(backbuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);

				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_swapchain->GetCurrentRtv().GetCpuAddress();
				D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_depthStencilView.GetCpuAddress();
				m_graphicsContext->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

				const float clearColor[] = { 0.15f, 0.15f, 0.16f, 1.0f };
				m_graphicsContext.ClearRtv(m_swapchain->GetCurrentRtv(), clearColor, 0, nullptr);
				m_graphicsContext.ClearDsv(m_depthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
			}

			// Wait for the copy context to finish here
			// TODO: Maybe write a cleaner way of waiting?
			RHICommandQueue* copyQueue = m_copyContext.GetQueue();
			m_graphicsContext.GetQueue()->WaitForValue(copyQueue->Signal(), copyQueue);

			{
				WARP_SCOPED_EVENT(&m_graphicsContext, "Renderer_RenderWorld_DispatchMeshes");

				RHIBuffer& constantBuffer = m_constantBuffers[frameIndex];

				UINT currentCbOffset = 0;
				RHIBufferAddress cbLightEnv(sizeof(HlslLightEnvironment), currentCbOffset);
				memcpy(cbLightEnv.GetCpuAddress(constantBuffer.GetCpuVirtualAddress()), &environment, sizeof(HlslLightEnvironment));
				currentCbOffset += cbLightEnv.SizeInBytes;
				
				m_graphicsContext.SetGraphicsRootSignature(m_basicRootSignature);
				m_graphicsContext->SetGraphicsRootConstantBufferView(BasicRootParameterIndex_CbLightEnv, cbLightEnv.GetGpuAddress(constantBuffer.GetGpuVirtualAddress()));

				HlslViewData viewData = HlslViewData{
					.ViewMatrix = cameraComponent.ViewMatrix,
					.ViewInvMatrix = cameraComponent.ViewInvMatrix,
					.ProjMatrix = cameraComponent.ProjMatrix,
				};
				RHIBufferAddress cbViewData(sizeof(HlslViewData), currentCbOffset);
				currentCbOffset += cbViewData.SizeInBytes;

				memcpy(cbViewData.GetCpuAddress(constantBuffer.GetCpuVirtualAddress()), &viewData, sizeof(HlslViewData));

				m_graphicsContext->SetGraphicsRootConstantBufferView(BasicRootParameterIndex_CbViewData, cbViewData.GetGpuAddress(constantBuffer.GetGpuVirtualAddress()));

				for (MeshInstance& meshInstance : meshInstances)
				{
					HlslDrawData drawData = HlslDrawData{
						.InstanceToWorld = meshInstance.InstanceToWorld,
						.NormalMatrix = meshInstance.NormalMatrix,
						.DrawFlags = meshInstance.DrawFlags
					};

					WARP_ASSERT(currentCbOffset < SizeOfGlobalCb, "Handle this! This is the time!");

					RHIBufferAddress cbDrawData(sizeof(HlslDrawData), currentCbOffset);
					currentCbOffset += cbDrawData.SizeInBytes;

					// TODO: The problem is that we update values here but do not wait for GPU to finish drawing, thus changing the values it uses... bad
					memcpy(cbDrawData.GetCpuAddress(constantBuffer.GetCpuVirtualAddress()), &drawData, sizeof(HlslDrawData));

					m_graphicsContext->SetGraphicsRootConstantBufferView(BasicRootParameterIndex_CbDrawData, cbDrawData.GetGpuAddress(constantBuffer.GetGpuVirtualAddress()));
					m_graphicsContext.SetPipelineState(m_basicPSO);

					MeshAsset* mesh = meshInstance.Mesh;
					for (size_t attributeIndex = 0; attributeIndex < EVertexAttributes_NumAttributes; ++attributeIndex)
					{
						// TODO: Add flags indicating meshe's attributes
						if (!mesh->HasAttributes(attributeIndex))
						{
							continue;
						}

						m_graphicsContext.AddTransitionBarrier(&mesh->Resources[attributeIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
						m_graphicsContext->SetGraphicsRootShaderResourceView(
							BasicRootParameterIndex_Positions + attributeIndex, 
							mesh->Resources[attributeIndex].GetGpuVirtualAddress());
					}

					std::array meshletResources = {
						&mesh->MeshletBuffer,
						&mesh->UniqueVertexIndicesBuffer,
						&mesh->PrimitiveIndicesBuffer
					};

					for (size_t i = 0; i < meshletResources.size(); ++i)
					{
						m_graphicsContext.AddTransitionBarrier(meshletResources[i], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
						m_graphicsContext->SetGraphicsRootShaderResourceView(BasicRootParameterIndex_Meshlets + i, meshletResources[i]->GetGpuVirtualAddress());
					}

					// We need a default texture for meshes with no basecolor. This is a better solution rather than doing float4 basecolor (as it is now in material)
					auto baseColor = mesh->Material.Manager->GetAs<TextureAsset>(mesh->Material.BaseColorProxy);
					if (baseColor)
					{
						m_graphicsContext->SetGraphicsRootDescriptorTable(BasicRootParameterIndex_BaseColor, baseColor->Srv.GetGpuAddress());
					}

					auto normalMap = mesh->Material.Manager->GetAs<TextureAsset>(mesh->Material.NormalMapProxy);
					if (normalMap)
					{
						m_graphicsContext->SetGraphicsRootDescriptorTable(BasicRootParameterIndex_NormalMap, normalMap->Srv.GetGpuAddress());
					}

					auto metalnessRoughnessMap = mesh->Material.Manager->GetAs<TextureAsset>(mesh->Material.MetalnessRoughnessMapProxy);
					if (metalnessRoughnessMap)
					{
						m_graphicsContext->SetGraphicsRootDescriptorTable(BasicRootParameterIndex_MetalnessRoughnessMap, metalnessRoughnessMap->Srv.GetGpuAddress());
					}

					m_graphicsContext.DispatchMesh(mesh->GetNumMeshlets(), 1, 1); // should be good enough for now
				}
			}
			// Indicate that we are ready to present
			m_graphicsContext.AddTransitionBarrier(backbuffer, D3D12_RESOURCE_STATE_PRESENT);
		}
		m_graphicsContext.Close();

		m_frameFenceValues[frameIndex] = m_graphicsContext.Execute(false);
		m_swapchain->Present(false);

		Device->EndFrame();
	}

	/*void Renderer::UploadSubresources(RHIResource* dest, std::vector<D3D12_SUBRESOURCE_DATA>& subresources, uint32_t subresourceOffset)
	{
		uint32_t numSubresources = static_cast<uint32_t>(subresources.size());

		RHIDevice* Device = m_device.get();
		RHIBuffer uploadBuffer = RHIBuffer(Device,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE,
			Device->GetCopyableBytes(dest, subresourceOffset, numSubresources));
		uploadBuffer.SetName(L"Renderer_UploadSubresources_IntermediateUploadBuffer");

		m_copyContext.Open();
		{
			m_copyContext.UploadSubresources(dest, &uploadBuffer, subresources, subresourceOffset);
		}
		m_copyContext.Close();
		UINT64 fenceValue = m_copyContext.Execute(false);

		m_uploadBufferTrackingContext.CleanupCompletedResources(m_copyContext.GetQueue());
		m_uploadBufferTrackingContext.AddTrackedResource(std::move(uploadBuffer), fenceValue);
	}*/

	//void Renderer::UploadToBuffer(RHIBuffer* dest, void* src, size_t numBytes)
	//{
	//	/*RHIDevice* Device = m_device.get();
	//	RHIBuffer uploadBuffer = RHIBuffer(Device,
	//		D3D12_HEAP_TYPE_UPLOAD,
	//		D3D12_RESOURCE_STATE_GENERIC_READ,
	//		D3D12_RESOURCE_FLAG_NONE, numBytes);
	//	uploadBuffer.SetName(L"Renderer_UploadToBuffer_IntermediateUploadBuffer");

	//	std::memcpy(uploadBuffer.GetCpuVirtualAddress<std::byte>(), src, numBytes);

	//	m_copyContext.CopyResource(dest, &uploadBuffer);

	//	m_uploadBufferTrackingContext.CleanupCompletedResources(m_copyContext.GetQueue());
	//	m_uploadBufferTrackingContext.AddTrackedResource(std::move(uploadBuffer), fenceValue);*/
	//}

	void Renderer::WaitForGfxOnFrameToFinish(uint32_t frameIndex)
	{
		m_graphicsContext.GetQueue()->HostWaitForValue(m_frameFenceValues[frameIndex]);
	}

	void Renderer::WaitForGfxToFinish()
	{
		m_graphicsContext.GetQueue()->HostWaitIdle();
	}

	void Renderer::InitDepthStencil()
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		desc.Width = m_swapchain->GetWidth();
		desc.Height = m_swapchain->GetHeight();
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		// Create depth-stencil texture2d
		CD3DX12_CLEAR_VALUE optimizedClearValue(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0);
		m_depthStencil = std::make_unique<RHITexture>(GetDevice(),
			D3D12_HEAP_TYPE_DEFAULT, 
			D3D12_RESOURCE_STATE_DEPTH_WRITE, 
			desc, 
			&optimizedClearValue);

		// Depth-stencil view
		RHIDevice* Device = m_device.get();
		m_depthStencilView = RHIDepthStencilView(m_device.get(), m_depthStencil.get(), nullptr, Device->GetDsvsHeap()->Allocate(1));
	}

	void Renderer::ResizeDepthStencil()
	{
		D3D12_RESOURCE_DESC desc = m_depthStencil->GetDesc();
		// these are the only changes after resize (for now atleast)
		desc.Width = m_swapchain->GetWidth();
		desc.Height = m_swapchain->GetHeight();

		CD3DX12_CLEAR_VALUE optimizedClearValue(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0);
		m_depthStencil->RecreateInPlace(D3D12_RESOURCE_STATE_DEPTH_WRITE, desc, &optimizedClearValue);
		m_depthStencilView.RecreateDescriptor(m_depthStencil.get());
	}

	bool Renderer::InitAssets()
	{
		std::string shaderPath = (Application::Get().GetShaderPath() / "Basic.hlsl").string();
		m_MSBasic = m_shaderCompiler.CompileShader(shaderPath, ShaderCompilationDesc("MSMain", EShaderModel::sm_6_5, EShaderType::Mesh));
		m_PSBasic = m_shaderCompiler.CompileShader(shaderPath, ShaderCompilationDesc("PSMain", EShaderModel::sm_6_5, EShaderType::Pixel));

		if (!m_MSBasic.HasBinary() || !m_PSBasic.HasBinary())
		{
			WARP_LOG_ERROR("Failed to init shaders");
			return false;
		}

		RHIDevice* Device = m_device.get();

		for (size_t i = 0; i < SimultaneousFrames; ++i)
		{
			m_constantBuffers[i] = RHIBuffer(Device,
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				D3D12_RESOURCE_FLAG_NONE, SizeOfGlobalCb);
			m_constantBuffers[i].SetName(L"GlobalCb");
		}

		// TODO: Figure out why MESH visibility wont work
		m_basicRootSignature = RHIRootSignature(GetDevice(), RHIRootSignatureDesc(1)
			.AddConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL)
			.AddConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL)
			.AddConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL)
			.AddShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_ALL) // Vertex attributes // 1
			.AddShaderResourceView(0, 1, D3D12_SHADER_VISIBILITY_ALL) // 2
			.AddShaderResourceView(0, 2, D3D12_SHADER_VISIBILITY_ALL) // 3
			.AddShaderResourceView(0, 3, D3D12_SHADER_VISIBILITY_ALL) // 4
			.AddShaderResourceView(0, 4, D3D12_SHADER_VISIBILITY_ALL) // 5
			.AddShaderResourceView(1, 0, D3D12_SHADER_VISIBILITY_ALL) // Meshlets // 6
			.AddShaderResourceView(2, 0, D3D12_SHADER_VISIBILITY_ALL) // uvIndices // 7
			.AddShaderResourceView(3, 0, D3D12_SHADER_VISIBILITY_ALL) // Prim indices // 8
			.AddDescriptorTable(RHIDescriptorTable(1).AddDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0), D3D12_SHADER_VISIBILITY_PIXEL) // BaseColor
			.AddDescriptorTable(RHIDescriptorTable(1).AddDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 1), D3D12_SHADER_VISIBILITY_PIXEL) // NormalMap
			.AddDescriptorTable(RHIDescriptorTable(1).AddDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 2), D3D12_SHADER_VISIBILITY_PIXEL) // MetalnessRoughness
			.AddStaticSampler(0, 0, D3D12_SHADER_VISIBILITY_PIXEL,
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP)
		);
		m_basicRootSignature.SetName(L"RootSignature_Basic");

		RHIMeshPipelineDesc psoDesc = {};
		psoDesc.RootSignature = m_basicRootSignature;
		psoDesc.MS = m_MSBasic.GetBinaryBytecode();
		psoDesc.PS = m_PSBasic.GetBinaryBytecode();
		psoDesc.Rasterizer.FrontCounterClockwise = TRUE; // TODO: We need this, because cube's triangle winding order is smhw ccw
		psoDesc.DepthStencil.DepthEnable = TRUE;
		psoDesc.DepthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencil.StencilEnable = FALSE;
		psoDesc.NumRTVs = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		m_basicPSO = RHIMeshPipelineState(GetDevice(), psoDesc);
		m_basicPSO.SetName(L"PSO_Basic");

		return true;
	}

}