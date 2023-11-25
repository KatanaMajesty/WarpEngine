#include "Renderer.h"

#include <cmath>
#include "../Util/String.h"
#include "../Core/Logger.h"
#include "../Core/Assert.h"
#include "../Core/Application.h"

// TODO: Temp, remove
#include "../Math/Math.h"
#include "RHI/PIXRuntime.h"

namespace Warp
{

	struct alignas(256) ConstantBuffer
	{
		Math::Matrix model;
		Math::Matrix view;
		Math::Matrix proj;
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

	void Renderer::RenderFrame(ModelAsset* model)
	{
		// Wait for inflight frame of the currently used backbuffer
		UINT frameIndex = m_swapchain->GetCurrentBackbufferIndex();
		WaitForGfxOnFrameToFinish(frameIndex);

		m_device->BeginFrame();

		m_graphicsContext.Open();
		{
			WARP_SCOPED_EVENT(&m_graphicsContext, fmt::format("Renderer::RenderFrame{}", frameIndex + 1));

			m_graphicsContext.SetPipelineState(m_cubePso);

			UINT width = m_swapchain->GetWidth();
			UINT height = m_swapchain->GetHeight();
			m_graphicsContext.SetViewport(0, 0, width, height);
			m_graphicsContext.SetScissorRect(0, 0, width, height);
			
			UINT currentBackbufferIndex = m_swapchain->GetCurrentBackbufferIndex();
			RHITexture* backbuffer = m_swapchain->GetBackbuffer(currentBackbufferIndex);

			// Indicate that the back buffer will be used as a render target.
			m_graphicsContext.AddTransitionBarrier(backbuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
			{
				WARP_SCOPED_EVENT(&m_graphicsContext, "Renderer::SetRTVs");

				// TODO: We do not change this to new API yet, although we want to
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_swapchain->GetCurrentRtv().GetCpuAddress();
				D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_depthStencilView.GetCpuAddress();
				m_graphicsContext->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

				const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
				m_graphicsContext.ClearRtv(m_swapchain->GetCurrentRtv(), clearColor, 0, nullptr);
				m_graphicsContext.ClearDsv(m_depthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
			}
			
			{
				WARP_SCOPED_EVENT(&m_graphicsContext, "Renderer::DispatchMesh");

				/*
				m_rootSignature = RHIRootSignature(GetDevice(), RHIRootSignatureDesc(1, 0)
			.AddConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL)
			.AddShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_ALL) // Vertex attributes // 1
			.AddShaderResourceView(0, 1, D3D12_SHADER_VISIBILITY_ALL) // 2
			.AddShaderResourceView(0, 2, D3D12_SHADER_VISIBILITY_ALL) // 3
			.AddShaderResourceView(0, 3, D3D12_SHADER_VISIBILITY_ALL) // 4
			.AddShaderResourceView(0, 4, D3D12_SHADER_VISIBILITY_ALL) // 5
			.AddShaderResourceView(1, 0, D3D12_SHADER_VISIBILITY_ALL) // Meshlets // 6
			.AddShaderResourceView(2, 0, D3D12_SHADER_VISIBILITY_ALL) // uvIndices // 7
			.AddShaderResourceView(3, 0, D3D12_SHADER_VISIBILITY_ALL) // Prim indices // 8
				*/
				// TODO: We need implicit state decay for our state tracking to work correctly
				
				m_graphicsContext.SetGraphicsRootSignature(m_rootSignature);
				m_graphicsContext->SetGraphicsRootConstantBufferView(0, m_constantBuffer.GetGpuVirtualAddress());
				StaticMesh& mesh = model->Meshes[0]; // Assert that cube has 1 mesh
				for (size_t i = 0; i < EVertexAttributes::NumAttributes; ++i)
				{
					// m_graphicsContext.AddTransitionBarrier(&mesh.StreamOfVertices.Resources[i], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
					m_graphicsContext->SetGraphicsRootShaderResourceView(i + 1, mesh.StreamOfVertices.Resources[i].GetGpuVirtualAddress());
				}
				// m_graphicsContext.AddTransitionBarrier(&mesh.MeshletBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				m_graphicsContext->SetGraphicsRootShaderResourceView(6, mesh.MeshletBuffer.GetGpuVirtualAddress());
				
				// m_graphicsContext.AddTransitionBarrier(&mesh.UniqueVertexIndicesBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				m_graphicsContext->SetGraphicsRootShaderResourceView(7, mesh.UniqueVertexIndicesBuffer.GetGpuVirtualAddress());
				
				// m_graphicsContext.AddTransitionBarrier(&mesh.PrimitiveIndicesBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				m_graphicsContext->SetGraphicsRootShaderResourceView(8, mesh.PrimitiveIndicesBuffer.GetGpuVirtualAddress());

				m_graphicsContext.DispatchMesh(mesh.StreamOfVertices.NumVertices / 128 + 1, 1, 1); // should be good enough for cube testing
			}
			// Indicate that the back buffer will now be used to present.
			m_graphicsContext.AddTransitionBarrier(backbuffer, D3D12_RESOURCE_STATE_PRESENT);
		}
		m_graphicsContext.Close();

		m_frameFenceValues[frameIndex] = m_graphicsContext.Execute(false);
		m_swapchain->Present(false);

		m_device->EndFrame();
	}

	void Renderer::Update(float timestep)
	{
		m_timeElapsed += timestep;

		float yaw = m_timeElapsed * 0.5f;
		float pitch = m_timeElapsed * 1.0f;
		float roll = m_timeElapsed * 0.75f;
		Math::Quaternion rotQuat = Math::Quaternion::CreateFromYawPitchRoll(Math::Vector3(yaw, pitch, roll));

		Math::Matrix translation = Math::Matrix::CreateTranslation(Math::Vector3(0.0f, 0.0f, -4.0f));
		Math::Matrix rotation = Math::Matrix::CreateFromQuaternion(rotQuat);
		Math::Matrix scale = Math::Matrix::CreateScale(1.0f);

		uint32_t width = m_swapchain->GetWidth();
		uint32_t height = m_swapchain->GetHeight();
		float aspectRatio = static_cast<float>(width) / height;

		ConstantBuffer cb;
		cb.model = scale * rotation * translation;
		cb.view = Math::Matrix();
		cb.proj = Math::Matrix::CreatePerspectiveFieldOfView(DirectX::XMConvertToRadians(90.0f), aspectRatio, 0.1f, 100.0f);

		ConstantBuffer* data = m_constantBuffer.GetCpuVirtualAddress<ConstantBuffer>();
		memcpy(data, &cb, sizeof(ConstantBuffer));
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
		m_depthStencil = std::make_unique<RHITexture>(GetDevice(),
			D3D12_HEAP_TYPE_DEFAULT, 
			D3D12_RESOURCE_STATE_DEPTH_WRITE, 
			desc, 
			CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0));

		// Depth-stencil view
		m_dsvHeap = std::make_unique<RHIDescriptorHeap>(GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
		m_depthStencilView = RHIDepthStencilView(m_device.get(), m_depthStencil.get(), nullptr, m_dsvHeap->Allocate(1));
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
		if (!InitShaders())
		{
			WARP_LOG_ERROR("Failed to init shaders");
			return false;
		}

		// TODO: Figure out why MESH visibility wont work
		m_rootSignature = RHIRootSignature(GetDevice(), RHIRootSignatureDesc(9, 0)
			.AddConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL)
			.AddShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_ALL) // Vertex attributes // 1
			.AddShaderResourceView(0, 1, D3D12_SHADER_VISIBILITY_ALL) // 2
			.AddShaderResourceView(0, 2, D3D12_SHADER_VISIBILITY_ALL) // 3
			.AddShaderResourceView(0, 3, D3D12_SHADER_VISIBILITY_ALL) // 4
			.AddShaderResourceView(0, 4, D3D12_SHADER_VISIBILITY_ALL) // 5
			.AddShaderResourceView(1, 0, D3D12_SHADER_VISIBILITY_ALL) // Meshlets // 6
			.AddShaderResourceView(2, 0, D3D12_SHADER_VISIBILITY_ALL) // uvIndices // 7
			.AddShaderResourceView(3, 0, D3D12_SHADER_VISIBILITY_ALL) // Prim indices // 8
		);
		m_rootSignature.SetName(L"RHIRootSign_Cube");

		RHIMeshPipelineDesc cubePsoDesc = {};
		cubePsoDesc.RootSignature = m_rootSignature;
		cubePsoDesc.MS = m_cubeMs.GetBinaryBytecode();
		cubePsoDesc.PS = m_cubePs.GetBinaryBytecode();
		cubePsoDesc.Rasterizer.FrontCounterClockwise = TRUE; // TODO: We need this, because cube's triangle winding order is smhw ccw
		cubePsoDesc.DepthStencil.DepthEnable = TRUE;
		cubePsoDesc.DepthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		cubePsoDesc.DepthStencil.StencilEnable = FALSE;
		cubePsoDesc.NumRTVs = 1;
		cubePsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		cubePsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		m_cubePso = RHIMeshPipelineState(GetDevice(), cubePsoDesc);
		m_cubePso.SetName(L"RHIMeshPso_Cube");

		// TODO: Remvoe this
		WARP_ASSERT(DirectX::XMVerifyCPUSupport(), "Cannot use DirectXMath for the provided CPU");
		
		ConstantBuffer cb;
		cb.model = DirectX::XMMatrixIdentity();
		cb.view = DirectX::XMMatrixIdentity();
		cb.proj = DirectX::XMMatrixIdentity();

		m_constantBuffer = m_device->CreateBuffer(sizeof(ConstantBuffer), sizeof(ConstantBuffer));
		m_constantBuffer.SetName(L"RHIBuffer_CubeCBV");

		ConstantBuffer* data = m_constantBuffer.GetCpuVirtualAddress<ConstantBuffer>();
		memcpy(data, &cb, sizeof(ConstantBuffer));

		WaitForGfxToFinish();
		return true;
	}

	bool Renderer::InitShaders()
	{
		std::string cubeShader = (Application::Get().GetShaderPath() / "Model.hlsl").string();
		ShaderCompilationDesc msShaderDesc = ShaderCompilationDesc("MSMain", EShaderModel::sm_6_5, EShaderType::Mesh);
		ShaderCompilationDesc psShaderDesc = ShaderCompilationDesc("PSMain", EShaderModel::sm_6_5, EShaderType::Pixel);

		m_cubeMs = m_shaderCompiler.CompileShader(cubeShader, msShaderDesc);
		m_cubePs = m_shaderCompiler.CompileShader(cubeShader, psShaderDesc);
		return m_cubeMs.HasBinary() && m_cubePs.HasBinary();
	}

	void Renderer::InitStupidBuffers(ModelAsset* asset)
	{
		StaticMesh& mesh = asset->Meshes[0]; // Hardcode, get a cube's mesh
		RHIDevice* device = m_device.get();

		uint32_t numDescriptors = 3 + EVertexAttributes::NumAttributes;
		m_cubeDescriptorHeap.reset(new RHIDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, numDescriptors, true));
		m_cubeDescriptors = m_cubeDescriptorHeap->Allocate(numDescriptors);
		WARP_ASSERT(!m_cubeDescriptors.IsNull());

		// Vertices
		{
			for (size_t i = 0; i < EVertexAttributes::NumAttributes; ++i)
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc;
				viewDesc.Format = DXGI_FORMAT_UNKNOWN;
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				viewDesc.Buffer.FirstElement = 0;
				viewDesc.Buffer.NumElements = mesh.StreamOfVertices.NumVertices;
				viewDesc.Buffer.StructureByteStride = mesh.StreamOfVertices.AttributeStrides[i];
				viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				m_attributeViews[i] = RHIShaderResourceView(device, &mesh.StreamOfVertices.Resources[i], &viewDesc, m_cubeDescriptors, i);
			}
		}

		// Meshlets
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc;
			viewDesc.Format = DXGI_FORMAT_UNKNOWN;
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			viewDesc.Buffer.FirstElement = 0;
			viewDesc.Buffer.NumElements = (UINT)mesh.Meshlets.size();
			viewDesc.Buffer.StructureByteStride = sizeof(Meshlet);
			viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			m_meshletView = RHIShaderResourceView(device, &mesh.MeshletBuffer, &viewDesc, m_cubeDescriptors, EVertexAttributes::NumAttributes + 0);
		}

		// Unique vertex indices
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc;
			viewDesc.Format = DXGI_FORMAT_R8_UINT;
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			viewDesc.Buffer.FirstElement = 0;
			viewDesc.Buffer.NumElements = (UINT)mesh.UniqueVertexIndices.size();
			viewDesc.Buffer.StructureByteStride = 0;
			viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			m_meshletView = RHIShaderResourceView(device, &mesh.MeshletBuffer, &viewDesc, m_cubeDescriptors, EVertexAttributes::NumAttributes + 1);
		}

		// Primitive indices
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc;
			viewDesc.Format = DXGI_FORMAT_UNKNOWN;
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			viewDesc.Buffer.FirstElement = 0;
			viewDesc.Buffer.NumElements = (UINT)mesh.Meshlets.size();
			viewDesc.Buffer.StructureByteStride = sizeof(Meshlet);
			viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			m_meshletView = RHIShaderResourceView(device, &mesh.MeshletBuffer, &viewDesc, m_cubeDescriptors, EVertexAttributes::NumAttributes + 2);
		}
		
	}

}