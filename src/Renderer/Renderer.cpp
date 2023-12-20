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

	struct alignas(256) HlslViewData
	{
		Math::Matrix ViewMatrix;
		Math::Matrix ProjMatrix;
	};

	struct alignas(256) HlslDrawData
	{
		Math::Matrix MeshToWorld;
	};

	// Represents indices of Basic.hlsl root signature
	enum BasicRootParameterIndex
	{
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
		, m_copyContext(RHICommandContext(L"RHICommandContext_Copy", m_device->GetCopyQueue()))
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
			MeshAsset* Mesh;
		};

		std::vector<MeshInstance> meshInstances;

		world->ViewOf<MeshComponent, TransformComponent>().each(
			[&meshInstances](MeshComponent& meshComponent, const TransformComponent& transformComponent)
			{
				MeshInstance& instance = meshInstances.emplace_back();

				Math::Matrix translation = Math::Matrix::CreateTranslation(transformComponent.Translation);
				Math::Matrix rotation = Math::Matrix::CreateFromYawPitchRoll(transformComponent.Rotation);
				Math::Matrix scale = Math::Matrix::CreateScale(transformComponent.Scaling);
				
				instance.InstanceToWorld = scale * rotation * translation;
				instance.Mesh = meshComponent.GetMesh();
			}
		);

		if (meshInstances.empty())
		{
			return;
		}

		Entity worldCamera = world->GetWorldCamera();
		const EulersCameraComponent& cameraComponent = worldCamera.GetComponent<EulersCameraComponent>();

		HlslViewData viewData = HlslViewData{
			.ViewMatrix = cameraComponent.ViewMatrix,
			.ProjMatrix = cameraComponent.ProjMatrix
		};
		memcpy(m_cbViewData.GetCpuVirtualAddress<HlslViewData>(), &viewData, sizeof(HlslViewData));

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

				const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
				m_graphicsContext.ClearRtv(m_swapchain->GetCurrentRtv(), clearColor, 0, nullptr);
				m_graphicsContext.ClearDsv(m_depthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
			}

			// Wait for the copy context to finish here
			// TODO: Maybe write a cleaner way of waiting?
			RHICommandQueue* copyQueue = m_copyContext.GetQueue();
			m_graphicsContext.GetQueue()->WaitForValue(copyQueue->Signal(), copyQueue);

			{
				WARP_SCOPED_EVENT(&m_graphicsContext, "Renderer_RenderWorld_DispatchMeshes");

				m_graphicsContext.SetGraphicsRootSignature(m_basicRootSignature);
				m_graphicsContext->SetGraphicsRootConstantBufferView(BasicRootParameterIndex_CbViewData, m_cbViewData.GetGpuVirtualAddress());

				for (MeshInstance& meshInstance : meshInstances)
				{
					HlslDrawData drawData = HlslDrawData{
						.MeshToWorld = meshInstance.InstanceToWorld
					};
					memcpy(m_cbDrawData.GetCpuVirtualAddress<HlslDrawData>(), &drawData, sizeof(HlslDrawData));

					m_graphicsContext->SetGraphicsRootConstantBufferView(BasicRootParameterIndex_CbDrawData, m_cbDrawData.GetGpuVirtualAddress());
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

					auto baseColor = mesh->Material.Manager->GetAs<TextureAsset>(mesh->Material.BaseColorProxy);
					m_graphicsContext->SetGraphicsRootDescriptorTable(BasicRootParameterIndex_BaseColor, baseColor->Srv.GetGpuAddress());

					auto normalMap = mesh->Material.Manager->GetAs<TextureAsset>(mesh->Material.NormalMapProxy);
					m_graphicsContext->SetGraphicsRootDescriptorTable(BasicRootParameterIndex_NormalMap, normalMap->Srv.GetGpuAddress());

					auto metalnessRoughnessMap = mesh->Material.Manager->GetAs<TextureAsset>(mesh->Material.MetalnessRoughnessMapProxy);
					m_graphicsContext->SetGraphicsRootDescriptorTable(BasicRootParameterIndex_MetalnessRoughnessMap, metalnessRoughnessMap->Srv.GetGpuAddress());

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

	void Renderer::UploadSubresources(RHIResource* dest, std::vector<D3D12_SUBRESOURCE_DATA>& subresources, uint32_t subresourceOffset)
	{
		std::erase_if(m_uploadResourceTrackedStates, [this](const UploadResourceTrackedState& trackedState) -> bool
			{
				return m_copyContext.GetQueue()->IsFenceComplete(trackedState.FenceValue);
			});

		uint32_t numSubresources = static_cast<uint32_t>(subresources.size());

		RHIDevice* Device = m_device.get();
		RHIBuffer uploadBuffer = RHIBuffer(Device,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE,
			0, Device->GetCopyableBytes(dest, subresourceOffset, numSubresources));
		uploadBuffer.SetName(L"Renderer_UploadSubresources_IntermediateUploadBuffer");

		m_copyContext.Open();
		{
			m_copyContext.UploadSubresources(dest, &uploadBuffer, subresources, subresourceOffset);
		}
		m_copyContext.Close();
		UINT64 fenceValue = m_copyContext.Execute(false);

		m_uploadResourceTrackedStates.emplace_back(std::move(uploadBuffer), fenceValue);
	}

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

		m_cbViewData = RHIBuffer(Device, 
			D3D12_HEAP_TYPE_UPLOAD, 
			D3D12_RESOURCE_STATE_GENERIC_READ, 
			D3D12_RESOURCE_FLAG_NONE, sizeof(HlslViewData), sizeof(HlslViewData));
		m_cbViewData.SetName(L"Cb_ViewData");

		m_cbDrawData = RHIBuffer(Device,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE, sizeof(HlslDrawData), sizeof(HlslDrawData));
		m_cbViewData.SetName(L"Cb_DrawData");

		// TODO: Figure out why MESH visibility wont work
		m_basicRootSignature = RHIRootSignature(GetDevice(), RHIRootSignatureDesc(1)
			.AddConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL)
			.AddConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL)
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