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
#include "../World/Entity.h"
#include "RHI/PIXRuntime.h"


namespace Warp
{

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
	enum BasicRootParamIdx
	{
		BasicRootParamIdx_CbViewData,
		BasicRootParamIdx_CbDrawData,
		BasicRootParamIdx_Positions,
		BasicRootParamIdx_Normals,
		BasicRootParamIdx_TexCoords,
		BasicRootParamIdx_Tangents,
		BasicRootParamIdx_Bitangents,
		BasicRootParamIdx_Meshlets,
		BasicRootParamIdx_UniqueVertexIndices,
		BasicRootParamIdx_PrimitiveIndices,
		BasicRootParamIdx_BaseColor,
		BasicRootParamIdx_NormalMap,
		BasicRootParamIdx_MetalnessRoughnessMap,
		BasicRootParamIdx_NumParams,
	};

	struct alignas(256) HlslDirShadowingViewData
	{
		Math::Matrix LightView;
		Math::Matrix LightProj;
	};

	struct alignas(256) HlslShadowingDrawData
	{
		Math::Matrix InstanceToWorld;
	};

	// Represents indices of DirectionalShadowing.hlsl root signature
	enum DirShadowingRootParamIdx
	{
		DirShadowingRootParamIdx_CbViewData,
		DirShadowingRootParamIdx_CbDrawData,
		DirShadowingRootParamIdx_Positions,
		DirShadowingRootParamIdx_Meshlets,
		DirShadowingRootParamIdx_UniqueVertexIndices,
		DirShadowingRootParamIdx_PrimitiveIndices,
		DirShadowingRootParamIdx_NumParams,
	};

	struct alignas(256) HlslDeferredLightingViewData
	{
		Math::Matrix ViewInv;
		Math::Matrix ProjInv;
	};

	// We want 16-byte boundary alignment for components
	// Previously we had 28-byte boundary here, which lead to issues
	struct alignas(16) HlslDirectionalLight
	{
		Math::Matrix LightView;
		Math::Matrix LightProj;
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

	enum DeferredLightingRootParamIdx
	{
		DeferredLightingRootParamIdx_CbViewData,
		DeferredLightingRootParamIdx_CbLightEnv,
		DeferredLightingRootParamIdx_GbufferAlbedo,
		DeferredLightingRootParamIdx_GbufferNormal,
		DeferredLightingRootParamIdx_GbufferRoughnessMetalness,
		DeferredLightingRootParamIdx_SceneDepth,
		DeferredLightingRootParamIdx_DirectionalShadowmaps,
		DeferredLightingRootParamIdx_NumParams,
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

		InitSceneDepth();
		InitGbuffers();
		InitDeferredLighting();

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

		ResizeSceneDepth();
		ResizeGbuffers();
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
		for (auto&& [entity, dirLightComponent] : world->GetEntityRegistry().view<DirectionalLightComponent>().each())
		{
			WARP_ASSERT(environment.NumDirectionalLights < HlslLightEnvironment::MaxDirectionalLights, "Too many dir lights! Handle this");

			// TODO: Currently we check if there is shadowmapping component. If no - add it
			Entity e = Entity(world, entity);
			if (dirLightComponent.CastsShadow && !e.HasComponents<DirectionalLightShadowmappingComponent>())
			{
				// Lazy-allocate if only we have any shadows to render
				if (m_directionalShadowingSrvs.IsNull())
				{
					m_directionalShadowingSrvs = m_device->GetViewHeap()->Allocate(HlslLightEnvironment::MaxDirectionalLights);
					WARP_ASSERT(m_directionalShadowingSrvs.IsValid());
				}

				// TODO: We should use camera's frustum for this
				Math::Vector3 DirLightPosition = dirLightComponent.Direction * -10.0f;

				DirectionalLightShadowmappingComponent& shadowComponent = e.AddComponent<DirectionalLightShadowmappingComponent>();
				shadowComponent.LightView = Math::Matrix::CreateLookAt(DirLightPosition, DirLightPosition + dirLightComponent.Direction, Math::Vector3(0.0f, 1.0f, 0.0f));
				shadowComponent.LightView.Invert(shadowComponent.LightInvView);

				shadowComponent.LightProj = Math::Matrix::CreateOrthographicOffCenter(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 200.0f);
				shadowComponent.LightProj.Invert(shadowComponent.LightInvProj);
				
				RHIDevice* Device = GetDevice();
				D3D12_RESOURCE_DESC shadowmapDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D24_UNORM_S8_UINT,
					2048, 2048,
					1, 1,
					1, 0,
					D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
				CD3DX12_CLEAR_VALUE optimizedClearValue(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0);
				shadowComponent.DepthMap = RHITexture(Device,
					D3D12_HEAP_TYPE_DEFAULT,
					D3D12_RESOURCE_STATE_DEPTH_WRITE,
					shadowmapDesc,
					&optimizedClearValue);

				// TODO: We need to remove these as well
				shadowComponent.DepthMapView = RHIDepthStencilView(Device, &shadowComponent.DepthMap, nullptr, Device->GetDsvsHeap()->Allocate(1));

				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.Texture2D = D3D12_TEX2D_SRV{
					.MostDetailedMip = 0,
					.MipLevels = UINT(-1),
					.PlaneSlice = 0,
					.ResourceMinLODClamp = 0.0f,
				};
				shadowComponent.DepthMapSrv = RHIShaderResourceView(Device, &shadowComponent.DepthMap, &srvDesc, m_directionalShadowingSrvs, environment.NumDirectionalLights);
			}

			DirectionalLightShadowmappingComponent& shadowComponent = e.GetComponent<DirectionalLightShadowmappingComponent>();
			environment.DirectionalLights[environment.NumDirectionalLights++] = HlslDirectionalLight{
				// TODO: LightView / LightProj are in DirectionalLightShadowmappingComponent which makes it a bit harder in general. Maybe all in 1 struct?
				.LightView = shadowComponent.LightView,
				.LightProj = shadowComponent.LightProj,
				.Intensity = dirLightComponent.Intensity,
				.Direction = dirLightComponent.Direction,
				.Radiance = dirLightComponent.Radiance,
			};
		}

		struct HlslShadowmappingTargets
		{
			uint32_t NumTargets = 0;
			std::array<DirectionalLightShadowmappingComponent*, HlslLightEnvironment::MaxDirectionalLights> Targets{};
		};

		// TODO: Maybe redo this in-place? Idk idk
		HlslShadowmappingTargets shadowmappingTargets;
		world->GetEntityRegistry().view<DirectionalLightShadowmappingComponent>().each(
			[&shadowmappingTargets](DirectionalLightShadowmappingComponent& shadowComponent)
			{
				WARP_ASSERT(shadowmappingTargets.NumTargets < HlslLightEnvironment::MaxDirectionalLights, "Too many dir lights! Handle this");
				shadowmappingTargets.Targets[shadowmappingTargets.NumTargets++] = &shadowComponent;
			}
		);

		Entity worldCamera = world->GetWorldCamera();
		const EulersCameraComponent& cameraComponent = worldCamera.GetComponent<EulersCameraComponent>();

		RHIDevice* Device = m_device.get();

		UINT frameIndex = m_swapchain->GetCurrentBackbufferIndex();
		UINT Width = m_swapchain->GetWidth();
		UINT Height = m_swapchain->GetHeight();
		WaitForGfxOnFrameToFinish(frameIndex);

		Device->BeginFrame();

		RHIBuffer& constantBuffer = m_constantBuffers[frameIndex];
		UINT currentCbOffset = 0;

		m_graphicsContext.Open();
		{
			WARP_SCOPED_EVENT(&m_graphicsContext, fmt::format("Renderer_RenderWorld_ShadowPasses_Frame{}", frameIndex + 1));

			{
				WARP_SCOPED_EVENT(&m_graphicsContext, "Renderer_RenderWorld_SetDescriptorHeaps");

				std::array<ID3D12DescriptorHeap*, 2> descriptorHeaps = {
					Device->GetSamplerHeap()->GetD3D12Heap(),
					Device->GetViewHeap()->GetD3D12Heap()
				};

				m_graphicsContext->SetDescriptorHeaps(static_cast<UINT>(descriptorHeaps.size()), descriptorHeaps.data());
			}

			for (uint32_t i = 0; i < shadowmappingTargets.NumTargets; ++i)
			{
				DirectionalLightShadowmappingComponent* shadowComponent = shadowmappingTargets.Targets[i];
				D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = shadowComponent->DepthMapView.GetCpuAddress();
				m_graphicsContext.AddTransitionBarrier(&shadowComponent->DepthMap, D3D12_RESOURCE_STATE_DEPTH_WRITE/*, 0*/ /*TODO: We assume that plane 0 (subresource 0) is for depth*/);
				m_graphicsContext->OMSetRenderTargets(0, nullptr, false, &dsvHandle);
				m_graphicsContext.ClearDsv(shadowComponent->DepthMapView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

				UINT shadowmapWidth = shadowComponent->DepthMap.GetWidth();
				UINT shadowmapHeight = shadowComponent->DepthMap.GetHeight();
				m_graphicsContext.SetViewport(0, 0, shadowmapWidth, shadowmapHeight);
				m_graphicsContext.SetScissorRect(0, 0, shadowmapWidth, shadowmapHeight);

				m_graphicsContext.SetGraphicsRootSignature(m_directionalShadowingSignature);
				m_graphicsContext.SetPipelineState(m_directionalShadowingPSO);

				HlslDirShadowingViewData viewData = HlslDirShadowingViewData{
					.LightView = shadowComponent->LightView,
					.LightProj = shadowComponent->LightProj,
				};
				RHIBuffer::Address cbViewData(&constantBuffer, sizeof(HlslDirShadowingViewData), currentCbOffset);
				currentCbOffset += cbViewData.SizeInBytes;
				memcpy(cbViewData.GetCpuAddress(), &viewData, sizeof(HlslDirShadowingViewData));

				m_graphicsContext->SetGraphicsRootConstantBufferView(DirShadowingRootParamIdx_CbViewData, cbViewData.GetGpuAddress());

				for (MeshInstance& meshInstance : meshInstances)
				{
					HlslShadowingDrawData drawData = HlslShadowingDrawData{
						.InstanceToWorld = meshInstance.InstanceToWorld,
					};

					WARP_ASSERT(currentCbOffset < SizeOfGlobalCb, "Handle this! This is the time!");

					RHIBuffer::Address cbDrawData(&constantBuffer, sizeof(HlslShadowingDrawData), currentCbOffset);
					currentCbOffset += cbDrawData.SizeInBytes;
					memcpy(cbDrawData.GetCpuAddress(), &drawData, sizeof(HlslShadowingDrawData));

					m_graphicsContext->SetGraphicsRootConstantBufferView(DirShadowingRootParamIdx_CbDrawData, cbDrawData.GetGpuAddress());

					MeshAsset* mesh = meshInstance.Mesh;
					m_graphicsContext.AddTransitionBarrier(&mesh->Resources[EVertexAttributes_Positions], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
					m_graphicsContext->SetGraphicsRootShaderResourceView(DirShadowingRootParamIdx_Positions, mesh->Resources[EVertexAttributes_Positions].GetGpuVirtualAddress());

					m_graphicsContext.AddTransitionBarrier(&mesh->MeshletBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
					m_graphicsContext->SetGraphicsRootShaderResourceView(DirShadowingRootParamIdx_Meshlets, mesh->MeshletBuffer.GetGpuVirtualAddress());

					m_graphicsContext.AddTransitionBarrier(&mesh->PrimitiveIndicesBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
					m_graphicsContext->SetGraphicsRootShaderResourceView(DirShadowingRootParamIdx_PrimitiveIndices, mesh->PrimitiveIndicesBuffer.GetGpuVirtualAddress());

					m_graphicsContext.AddTransitionBarrier(&mesh->UniqueVertexIndicesBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
					m_graphicsContext->SetGraphicsRootShaderResourceView(DirShadowingRootParamIdx_UniqueVertexIndices, mesh->UniqueVertexIndicesBuffer.GetGpuVirtualAddress());

					m_graphicsContext.DispatchMesh(mesh->GetNumMeshlets(), 1, 1); // should be good enough for now
				}
			}
		};
		m_graphicsContext.Close();
		UINT64 dirShadowingPassFenceValue = m_graphicsContext.Execute(false);
		m_frameFenceValues[frameIndex] = dirShadowingPassFenceValue;

		m_graphicsContext.Open();
		{
			WARP_SCOPED_EVENT(&m_graphicsContext, fmt::format("Renderer_RenderWorld_BasePass_Frame{}", frameIndex + 1));
			m_graphicsContext.SetViewport(0, 0, Width, Height);
			m_graphicsContext.SetScissorRect(0, 0, Width, Height);

			{
				WARP_SCOPED_EVENT(&m_graphicsContext, "Renderer_RenderWorld_SetDescriptorHeaps");

				std::array<ID3D12DescriptorHeap*, 2> descriptorHeaps = {
					Device->GetSamplerHeap()->GetD3D12Heap(),
					Device->GetViewHeap()->GetD3D12Heap()
				};

				m_graphicsContext->SetDescriptorHeaps(static_cast<UINT>(descriptorHeaps.size()), descriptorHeaps.data());
			}

			// Wait for the copy context to finish here
			// TODO: Maybe write a cleaner way of waiting?
			RHICommandQueue* copyQueue = m_copyContext.GetQueue();
			m_graphicsContext.GetQueue()->WaitForValue(copyQueue->Signal(), copyQueue);

			{
				for (uint32_t i = 0; i < eGbufferType_NumTypes; ++i)
					m_graphicsContext.AddTransitionBarrier(&m_gbuffers[i].Buffer, D3D12_RESOURCE_STATE_RENDER_TARGET);

				// Add scene depth state transition
				m_graphicsContext.AddTransitionBarrier(&m_sceneDepth, D3D12_RESOURCE_STATE_DEPTH_WRITE/*, 0*/ /*TODO: We assume that plane 0 (subresource 0) is for depth*/);

				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_gbufferRtvs.GetCpuAddress();
				D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_sceneDepthDsv.GetCpuAddress();
				m_graphicsContext->OMSetRenderTargets(eGbufferType_NumTypes, &rtvHandle, true, &dsvHandle);

				const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
				m_graphicsContext.ClearRtv(m_gbuffers[eGbufferType_Albedo].Rtv, clearColor, 0, nullptr);
				m_graphicsContext.ClearRtv(m_gbuffers[eGbufferType_Normal].Rtv, clearColor, 0, nullptr);
				m_graphicsContext.ClearRtv(m_gbuffers[eGbufferType_RoughnessMetalness].Rtv, clearColor, 0, nullptr);
				m_graphicsContext.ClearDsv(m_sceneDepthDsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

				m_graphicsContext.SetGraphicsRootSignature(m_baseRootSignature);
				m_graphicsContext.SetPipelineState(m_basePSO);

				HlslViewData viewData = HlslViewData{
					.ViewMatrix = cameraComponent.ViewMatrix,
					.ViewInvMatrix = cameraComponent.ViewInvMatrix,
					.ProjMatrix = cameraComponent.ProjMatrix,
				};
				RHIBuffer::Address cbViewData(&constantBuffer, sizeof(HlslViewData), currentCbOffset);
				currentCbOffset += cbViewData.SizeInBytes;
				memcpy(cbViewData.GetCpuAddress(), &viewData, sizeof(HlslViewData));

				m_graphicsContext->SetGraphicsRootConstantBufferView(BasicRootParamIdx_CbViewData, cbViewData.GetGpuAddress());

				// TODO: This will be moved to deferred pass
				// Bind shadow maps
				//if (shadowmappingTargets.NumTargets > 0)
				//{
				//	// TODO: Should we wait here?
				//	DirectionalLightShadowmappingComponent* target = shadowmappingTargets.Targets[0];
				//	m_graphicsContext.AddTransitionBarrier(&target->DepthMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				//	m_graphicsContext.GetQueue()->WaitForValue(dirShadowingPassFenceValue);
				//	m_graphicsContext->SetGraphicsRootDescriptorTable(BasicRootParamIdx_DirectionalShadowmaps, m_directionalShadowingSrvs.GetGpuAddress());
				//}
				//else
				//{
				//	WARP_ASSERT(false, "No Lights! Should handle using scene cb! -> Currently not implemented");
				//}

				for (MeshInstance& meshInstance : meshInstances)
				{
					HlslDrawData drawData = HlslDrawData{
						.InstanceToWorld = meshInstance.InstanceToWorld,
						.NormalMatrix = meshInstance.NormalMatrix,
						.DrawFlags = meshInstance.DrawFlags
					};

					WARP_ASSERT(currentCbOffset < SizeOfGlobalCb, "Handle this! This is the time!");

					RHIBuffer::Address cbDrawData(&constantBuffer, sizeof(HlslDrawData), currentCbOffset);
					currentCbOffset += cbDrawData.SizeInBytes;
					memcpy(cbDrawData.GetCpuAddress(), &drawData, sizeof(HlslDrawData));

					m_graphicsContext->SetGraphicsRootConstantBufferView(BasicRootParamIdx_CbDrawData, cbDrawData.GetGpuAddress());

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
							BasicRootParamIdx_Positions + attributeIndex,
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
						m_graphicsContext->SetGraphicsRootShaderResourceView(BasicRootParamIdx_Meshlets + i, meshletResources[i]->GetGpuVirtualAddress());
					}

					// We need a default texture for meshes with no basecolor. This is a better solution rather than doing float4 basecolor (as it is now in material)
					auto baseColor = mesh->Material.Manager->GetAs<TextureAsset>(mesh->Material.BaseColorProxy);
					if (baseColor)
					{
						m_graphicsContext->SetGraphicsRootDescriptorTable(BasicRootParamIdx_BaseColor, baseColor->Srv.GetGpuAddress());
					}

					auto normalMap = mesh->Material.Manager->GetAs<TextureAsset>(mesh->Material.NormalMapProxy);
					if (normalMap)
					{
						m_graphicsContext->SetGraphicsRootDescriptorTable(BasicRootParamIdx_NormalMap, normalMap->Srv.GetGpuAddress());
					}

					auto metalnessRoughnessMap = mesh->Material.Manager->GetAs<TextureAsset>(mesh->Material.MetalnessRoughnessMapProxy);
					if (metalnessRoughnessMap)
					{
						m_graphicsContext->SetGraphicsRootDescriptorTable(BasicRootParamIdx_MetalnessRoughnessMap, metalnessRoughnessMap->Srv.GetGpuAddress());
					}

					m_graphicsContext.DispatchMesh(mesh->GetNumMeshlets(), 1, 1); // should be good enough for now
				}
			}
		}
		m_graphicsContext.Close();
		m_frameFenceValues[frameIndex] = m_graphicsContext.Execute(false);

		RHITexture* backbuffer = m_swapchain->GetBackbuffer(frameIndex);
		
		// Deferred pass
		m_graphicsContext.Open();
		{
			WARP_SCOPED_EVENT(&m_graphicsContext, fmt::format("Renderer_RenderWorld_DeferredPass_Frame{}", frameIndex + 1));
			m_graphicsContext.SetViewport(0, 0, Width, Height);
			m_graphicsContext.SetScissorRect(0, 0, Width, Height);

			{
				WARP_SCOPED_EVENT(&m_graphicsContext, "Renderer_RenderWorld_SetDescriptorHeaps");

				std::array<ID3D12DescriptorHeap*, 2> descriptorHeaps = {
					Device->GetSamplerHeap()->GetD3D12Heap(),
					Device->GetViewHeap()->GetD3D12Heap()
				};

				m_graphicsContext->SetDescriptorHeaps(static_cast<UINT>(descriptorHeaps.size()), descriptorHeaps.data());
			}

			// Actual drawing
			{
				m_graphicsContext.AddTransitionBarrier(backbuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);

				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_swapchain->GetCurrentRtv().GetCpuAddress();
				m_graphicsContext->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

				constexpr float clearcolor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
				m_graphicsContext.ClearRtv(m_swapchain->GetCurrentRtv(), clearcolor, 0, nullptr);
				
				m_graphicsContext.SetGraphicsRootSignature(m_deferredLightingSignature);
				m_graphicsContext.SetPipelineState(m_deferredLightingPSO);
				m_graphicsContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // TODO: Why the fuck is this even a thing?

				HlslDeferredLightingViewData viewData = HlslDeferredLightingViewData{
					.ViewInv = cameraComponent.ViewInvMatrix,
					.ProjInv = cameraComponent.ProjInvMatrix,
				};
				WARP_ASSERT(currentCbOffset < SizeOfGlobalCb, "Handle this! This is the time!");

				RHIBuffer::Address cbViewData(&constantBuffer, sizeof(HlslDeferredLightingViewData), currentCbOffset);
				currentCbOffset += cbViewData.SizeInBytes;
				memcpy(cbViewData.GetCpuAddress(), &viewData, sizeof(HlslDeferredLightingViewData));

				m_graphicsContext->SetGraphicsRootConstantBufferView(DeferredLightingRootParamIdx_CbViewData, cbViewData.GetGpuAddress());

				RHIBuffer::Address cbLightEnv(&constantBuffer, sizeof(HlslLightEnvironment), currentCbOffset);
				currentCbOffset += cbLightEnv.SizeInBytes;
				memcpy(cbLightEnv.GetCpuAddress(), &environment, sizeof(HlslLightEnvironment));
				m_graphicsContext->SetGraphicsRootConstantBufferView(DeferredLightingRootParamIdx_CbLightEnv, cbLightEnv.GetGpuAddress());

				// Transition and set gbuffers
				for (uint32_t i = 0; i < eGbufferType_NumTypes; ++i)
				{
					m_graphicsContext.AddTransitionBarrier(&m_gbuffers[i].Buffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				}
				m_graphicsContext->SetGraphicsRootDescriptorTable(DeferredLightingRootParamIdx_GbufferAlbedo, m_gbuffers[eGbufferType_Albedo].Srv.GetGpuAddress());
				m_graphicsContext->SetGraphicsRootDescriptorTable(DeferredLightingRootParamIdx_GbufferNormal, m_gbuffers[eGbufferType_Normal].Srv.GetGpuAddress());
				m_graphicsContext->SetGraphicsRootDescriptorTable(DeferredLightingRootParamIdx_GbufferRoughnessMetalness, m_gbuffers[eGbufferType_RoughnessMetalness].Srv.GetGpuAddress());

				// Bind scene depth as Srv
				m_graphicsContext.AddTransitionBarrier(&m_sceneDepth, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE/*, 0*/ /*TODO: We assume that plane 0 (subresource 0) is for depth*/);
				m_graphicsContext->SetGraphicsRootDescriptorTable(DeferredLightingRootParamIdx_SceneDepth, m_sceneDepthSrv.GetGpuAddress());

				// Transition and set directional shadowmaps
				for (uint32_t i = 0; i < shadowmappingTargets.NumTargets; ++i)
				{
					m_graphicsContext.AddTransitionBarrier(&shadowmappingTargets.Targets[i]->DepthMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				}
				m_graphicsContext->SetGraphicsRootDescriptorTable(DeferredLightingRootParamIdx_DirectionalShadowmaps, m_directionalShadowingSrvs.GetGpuAddress());

				// Fullscreen triangle
				m_graphicsContext.DrawInstanced(3, 1, 0, 0);
				m_graphicsContext.AddTransitionBarrier(backbuffer, D3D12_RESOURCE_STATE_PRESENT);
			}
		}
		m_graphicsContext.Close();
		m_frameFenceValues[frameIndex] = m_graphicsContext.Execute(false);

		m_swapchain->Present(false);

		Device->EndFrame();
	}

	void Renderer::WaitForGfxOnFrameToFinish(uint32_t frameIndex)
	{
		m_graphicsContext.GetQueue()->HostWaitForValue(m_frameFenceValues[frameIndex]);
	}

	void Renderer::WaitForGfxToFinish()
	{
		m_graphicsContext.GetQueue()->HostWaitIdle();
	}

	bool Renderer::InitAssets()
	{
		RHIDevice* Device = m_device.get();

		// Allocate global CBuffers
		for (size_t i = 0; i < SimultaneousFrames; ++i)
		{
			m_constantBuffers[i] = RHIBuffer(Device,
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				D3D12_RESOURCE_FLAG_NONE, SizeOfGlobalCb);
			m_constantBuffers[i].SetName(L"GlobalCb");
		}

		{
			std::string shaderPath;

			shaderPath = (Application::Get().GetShaderPath() / "Base.hlsl").string();
			m_MSBase = m_shaderCompiler.CompileShader(shaderPath, ShaderCompilationDesc("MSMain", EShaderModel::sm_6_5, EShaderType::Mesh));
			m_PSBase = m_shaderCompiler.CompileShader(shaderPath, ShaderCompilationDesc("PSMain", EShaderModel::sm_6_5, EShaderType::Pixel));
			WARP_ASSERT(
				m_MSBase.HasBinary() && 
				m_PSBase.HasBinary(), "Failed to compile Base.hlsl");

			shaderPath = (Application::Get().GetShaderPath() / "DirectionalShadowing.hlsl").string();
			m_MSDirectionalShadowing = m_shaderCompiler.CompileShader(shaderPath, ShaderCompilationDesc("MSMain", EShaderModel::sm_6_5, EShaderType::Mesh));
			WARP_ASSERT(m_MSDirectionalShadowing.HasBinary() && "Failed to compile DirectionalShadowing.hlsl");
		}

		// TODO: Figure out why MESH visibility wont work
		m_baseRootSignature = RHIRootSignature(Device, RHIRootSignatureDesc(BasicRootParamIdx_NumParams)
			// Cbvs
			.SetConstantBufferView(BasicRootParamIdx_CbViewData, 0, 0, D3D12_SHADER_VISIBILITY_ALL)
			.SetConstantBufferView(BasicRootParamIdx_CbDrawData, 1, 0, D3D12_SHADER_VISIBILITY_ALL)
			// VertexAttributes
			.SetShaderResourceView(BasicRootParamIdx_Positions, 0, 0, D3D12_SHADER_VISIBILITY_ALL)
			.SetShaderResourceView(BasicRootParamIdx_Normals, 0, 1, D3D12_SHADER_VISIBILITY_ALL)
			.SetShaderResourceView(BasicRootParamIdx_TexCoords, 0, 2, D3D12_SHADER_VISIBILITY_ALL)
			.SetShaderResourceView(BasicRootParamIdx_Tangents, 0, 3, D3D12_SHADER_VISIBILITY_ALL)
			.SetShaderResourceView(BasicRootParamIdx_Bitangents, 0, 4, D3D12_SHADER_VISIBILITY_ALL)
			// Meshlets
			.SetShaderResourceView(BasicRootParamIdx_Meshlets, 1, 0, D3D12_SHADER_VISIBILITY_ALL)
			.SetShaderResourceView(BasicRootParamIdx_UniqueVertexIndices, 2, 0, D3D12_SHADER_VISIBILITY_ALL)
			.SetShaderResourceView(BasicRootParamIdx_PrimitiveIndices, 3, 0, D3D12_SHADER_VISIBILITY_ALL)
			// SRVs
			.SetDescriptorTable(BasicRootParamIdx_BaseColor, RHIDescriptorTable(1).AddDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0), D3D12_SHADER_VISIBILITY_PIXEL)
			.SetDescriptorTable(BasicRootParamIdx_NormalMap, RHIDescriptorTable(1).AddDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 1), D3D12_SHADER_VISIBILITY_PIXEL)
			.SetDescriptorTable(BasicRootParamIdx_MetalnessRoughnessMap, RHIDescriptorTable(1).AddDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 2), D3D12_SHADER_VISIBILITY_PIXEL)
			.AddStaticSampler(0, 0, D3D12_SHADER_VISIBILITY_PIXEL,
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
				D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
				D3D12_TEXTURE_ADDRESS_MODE_MIRROR)
		);
		m_baseRootSignature.SetName(L"RootSignature_Base");

		RHIMeshPipelineDesc psoDesc = {};
		psoDesc.RootSignature = m_baseRootSignature;
		psoDesc.MS = m_MSBase.GetBinaryBytecode();
		psoDesc.PS = m_PSBase.GetBinaryBytecode();
		psoDesc.Rasterizer.FrontCounterClockwise = TRUE; // TODO: We need this, because cube's triangle winding order is smhw ccw
		psoDesc.DepthStencil.DepthEnable = TRUE;
		psoDesc.DepthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencil.StencilEnable = FALSE;
		psoDesc.NumRTVs = eGbufferType_NumTypes;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_SNORM;
		psoDesc.RTVFormats[2] = DXGI_FORMAT_R8G8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		m_basePSO = RHIMeshPipelineState(Device, psoDesc);
		m_basePSO.SetName(L"PSO_Base");

		// TODO: D3D12_SHADER_VISIBILITY_MESH somehow crashes gpu. Idk why though
		m_directionalShadowingSignature = RHIRootSignature(Device, RHIRootSignatureDesc(DirShadowingRootParamIdx_NumParams)
			// Cbvs
			.SetConstantBufferView(DirShadowingRootParamIdx_CbViewData, 0, 0, D3D12_SHADER_VISIBILITY_ALL)
			.SetConstantBufferView(DirShadowingRootParamIdx_CbDrawData, 1, 0, D3D12_SHADER_VISIBILITY_ALL)
			// Srvs
			.SetShaderResourceView(DirShadowingRootParamIdx_Positions, 0, 0, D3D12_SHADER_VISIBILITY_ALL)
			.SetShaderResourceView(DirShadowingRootParamIdx_Meshlets, 1, 0, D3D12_SHADER_VISIBILITY_ALL)
			.SetShaderResourceView(DirShadowingRootParamIdx_UniqueVertexIndices, 2, 0, D3D12_SHADER_VISIBILITY_ALL)
			.SetShaderResourceView(DirShadowingRootParamIdx_PrimitiveIndices, 3, 0, D3D12_SHADER_VISIBILITY_ALL)
		);
		m_directionalShadowingSignature.SetName(L"RootSignature_DirectionalShadowing");

		psoDesc = {};
		psoDesc.RootSignature = m_directionalShadowingSignature;
		psoDesc.MS = m_MSDirectionalShadowing.GetBinaryBytecode();
		psoDesc.Rasterizer.FrontCounterClockwise = TRUE; // TODO: We need this, because cube's triangle winding order is smhw ccw
		psoDesc.Rasterizer.DepthBias = 0;
		psoDesc.Rasterizer.DepthBiasClamp = 0.0f;
		psoDesc.Rasterizer.SlopeScaledDepthBias = 2.0f;
		psoDesc.DepthStencil.DepthEnable = TRUE;
		psoDesc.DepthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencil.StencilEnable = FALSE;
		psoDesc.NumRTVs = 0;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		m_directionalShadowingPSO = RHIMeshPipelineState(Device, psoDesc);
		m_directionalShadowingPSO.SetName(L"PSO_DirectionalShadowing");

		return true;
	}

	void Renderer::InitSceneDepth()
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
		m_sceneDepth = RHITexture(GetDevice(),
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			desc,
			&optimizedClearValue);
		m_sceneDepth.SetName(L"SceneDepth");

		// Depth-stencil view
		m_sceneDepthDsv = RHIDepthStencilView(GetDevice(), &m_sceneDepth, nullptr, GetDevice()->GetDsvsHeap()->Allocate(1));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D = D3D12_TEX2D_SRV{
			.MostDetailedMip = 0,
			.MipLevels = UINT(-1),
			.PlaneSlice = 0,
			.ResourceMinLODClamp = 0.0f,
		};
		m_sceneDepthSrv = RHIShaderResourceView(GetDevice(), &m_sceneDepth, &srvDesc, GetDevice()->GetViewHeap()->Allocate(1));
	}

	void Renderer::ResizeSceneDepth()
	{
		D3D12_RESOURCE_DESC desc = m_sceneDepth.GetDesc();
		// these are the only changes after resize (for now atleast)
		desc.Width = m_swapchain->GetWidth();
		desc.Height = m_swapchain->GetHeight();

		CD3DX12_CLEAR_VALUE optimizedClearValue(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0);
		m_sceneDepth.RecreateInPlace(D3D12_RESOURCE_STATE_DEPTH_WRITE, desc, &optimizedClearValue);
		m_sceneDepth.SetName(L"SceneDepth"); // TODO: Do we need to do this on resize?
		m_sceneDepthDsv.RecreateDescriptor(&m_sceneDepth);
		m_sceneDepthSrv.RecreateDescriptor(&m_sceneDepth);
	}

	void Renderer::InitGbuffers()
	{
		RHIDevice* Device = GetDevice();
		UINT width = m_swapchain->GetWidth();
		UINT height = m_swapchain->GetHeight();

		// Albedo Gbuffer
		{
			DXGI_FORMAT gbufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
			// TODO: Use 16bit per-channel format when HDR
			D3D12_RESOURCE_DESC albedoDesc = CD3DX12_RESOURCE_DESC::Tex2D(gbufferFormat, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
			FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(gbufferFormat, clearColor);
			m_gbuffers[eGbufferType_Albedo].Buffer = RHITexture(Device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, albedoDesc, &clearValue);
		}
		
		// Normal GBuffer
		{
			DXGI_FORMAT gbufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
			D3D12_RESOURCE_DESC normalDesc = CD3DX12_RESOURCE_DESC::Tex2D(gbufferFormat, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
			FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(gbufferFormat, clearColor);
			m_gbuffers[eGbufferType_Normal].Buffer = RHITexture(Device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, normalDesc, &clearValue);
		}

		// RoughnessMetalness Gbuffer
		{
			DXGI_FORMAT gbufferFormat = DXGI_FORMAT_R8G8_UNORM;
			D3D12_RESOURCE_DESC roughnessMetalnessDesc = CD3DX12_RESOURCE_DESC::Tex2D(gbufferFormat, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
			FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(gbufferFormat, clearColor);
			m_gbuffers[eGbufferType_RoughnessMetalness].Buffer = RHITexture(Device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, roughnessMetalnessDesc, &clearValue);
		}

		// Allocate descriptors
		WARP_ASSERT(m_gbufferRtvs.IsNull());
		m_gbufferRtvs = Device->GetRtvsHeap()->Allocate(eGbufferType_NumTypes);
		WARP_ASSERT(m_gbufferRtvs.IsValid());

		
		WARP_ASSERT(m_gbufferSrvs.IsNull());
		m_gbufferSrvs = Device->GetViewHeap()->Allocate(eGbufferType_NumTypes);
		WARP_ASSERT(m_gbufferSrvs.IsValid());

		for (uint32_t i = 0; i < eGbufferType_NumTypes; ++i)
		{
			m_gbuffers[i].Rtv = RHIRenderTargetView(Device, &m_gbuffers[i].Buffer, nullptr, m_gbufferRtvs, i);
			m_gbuffers[i].Srv = RHIShaderResourceView(Device, &m_gbuffers[i].Buffer, nullptr, m_gbufferSrvs, i);
		}
	}

	void Renderer::ResizeGbuffers()
	{
		RHIDevice* Device = GetDevice();
		UINT width = m_swapchain->GetWidth();
		UINT height = m_swapchain->GetHeight();

		// Albedo Gbuffer
		{
			DXGI_FORMAT gbufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
			// TODO: Use 16bit per-channel format when HDR
			D3D12_RESOURCE_DESC albedoDesc = CD3DX12_RESOURCE_DESC::Tex2D(gbufferFormat, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
			FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(gbufferFormat, clearColor);
			m_gbuffers[eGbufferType_Albedo].Buffer.RecreateInPlace(D3D12_RESOURCE_STATE_COMMON, albedoDesc, &clearValue);
		}

		// Normal GBuffer
		{
			DXGI_FORMAT gbufferFormat = DXGI_FORMAT_R8G8B8A8_SNORM;
			D3D12_RESOURCE_DESC normalDesc = CD3DX12_RESOURCE_DESC::Tex2D(gbufferFormat, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
			FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(gbufferFormat, clearColor);
			m_gbuffers[eGbufferType_Normal].Buffer.RecreateInPlace(D3D12_RESOURCE_STATE_COMMON, normalDesc, &clearValue);
		}

		// RoughnessMetalness Gbuffer
		{
			DXGI_FORMAT gbufferFormat = DXGI_FORMAT_R8G8_UNORM;
			D3D12_RESOURCE_DESC roughnessMetalnessDesc = CD3DX12_RESOURCE_DESC::Tex2D(gbufferFormat, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
			FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(gbufferFormat, clearColor);
			m_gbuffers[eGbufferType_RoughnessMetalness].Buffer.RecreateInPlace(D3D12_RESOURCE_STATE_COMMON, roughnessMetalnessDesc, &clearValue);
		}

		WARP_ASSERT(m_gbufferRtvs.IsValid() && m_gbufferSrvs.IsValid(), "They should have been allocated in InitGBuffers()");

		for (uint32_t i = 0; i < eGbufferType_NumTypes; ++i)
		{
			m_gbuffers[i].Rtv = RHIRenderTargetView(Device, &m_gbuffers[i].Buffer, nullptr, m_gbufferRtvs, i);
			m_gbuffers[i].Srv = RHIShaderResourceView(Device, &m_gbuffers[i].Buffer, nullptr, m_gbufferSrvs, i);
		}
	}

	void Renderer::InitDeferredLighting()
	{
		RHIDevice* Device = GetDevice();
		m_deferredLightingSignature = RHIRootSignature(Device, RHIRootSignatureDesc(DeferredLightingRootParamIdx_NumParams)
			.SetConstantBufferView(DeferredLightingRootParamIdx_CbViewData, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL)
			.SetConstantBufferView(DeferredLightingRootParamIdx_CbLightEnv, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL)
			.SetDescriptorTable(DeferredLightingRootParamIdx_GbufferAlbedo, RHIDescriptorTable(1).AddDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0), D3D12_SHADER_VISIBILITY_PIXEL)
			.SetDescriptorTable(DeferredLightingRootParamIdx_GbufferNormal, RHIDescriptorTable(1).AddDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 1), D3D12_SHADER_VISIBILITY_PIXEL)
			.SetDescriptorTable(DeferredLightingRootParamIdx_GbufferRoughnessMetalness, RHIDescriptorTable(1).AddDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 2), D3D12_SHADER_VISIBILITY_PIXEL)
			.SetDescriptorTable(DeferredLightingRootParamIdx_SceneDepth, RHIDescriptorTable(1).AddDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0), D3D12_SHADER_VISIBILITY_PIXEL)
			.SetDescriptorTable(DeferredLightingRootParamIdx_DirectionalShadowmaps, 
				RHIDescriptorTable(1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE).AddDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, HlslLightEnvironment::MaxDirectionalLights, 2, 0), D3D12_SHADER_VISIBILITY_PIXEL)
			.AddStaticSampler(0, 0, D3D12_SHADER_VISIBILITY_PIXEL,
				D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP)
			.AddStaticSampler(1, 0, D3D12_SHADER_VISIBILITY_PIXEL,
				D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
				D3D12_TEXTURE_ADDRESS_MODE_BORDER,
				D3D12_TEXTURE_ADDRESS_MODE_BORDER,
				D3D12_TEXTURE_ADDRESS_MODE_BORDER));
		m_deferredLightingSignature.SetName(L"DeferredLightingSignature");

		std::string shaderPath = (Application::Get().GetShaderPath() / "Deferred.hlsl").string();
		m_VSDeferredLighting = m_shaderCompiler.CompileShader(shaderPath, ShaderCompilationDesc("VSMain", EShaderModel::sm_6_5, EShaderType::Vertex));
		m_PSDeferredLighting = m_shaderCompiler.CompileShader(shaderPath, ShaderCompilationDesc("PSMain", EShaderModel::sm_6_5, EShaderType::Pixel));
		WARP_ASSERT(
			m_VSDeferredLighting.HasBinary() &&
			m_PSDeferredLighting.HasBinary(), "Failed to compile Base.hlsl");

		RHIGraphicsPipelineDesc psoDesc = {};
		psoDesc.RootSignature = m_deferredLightingSignature;
		psoDesc.VS = m_VSDeferredLighting.GetBinaryBytecode();
		psoDesc.PS = m_PSDeferredLighting.GetBinaryBytecode();
		psoDesc.DepthStencil.DepthEnable = FALSE;
		psoDesc.DepthStencil.StencilEnable = FALSE;
		psoDesc.NumRTVs = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		m_deferredLightingPSO = RHIGraphicsPipelineState(GetDevice(), psoDesc);
		m_deferredLightingPSO.SetName(L"PSO_DeferredLighting");
	}

}