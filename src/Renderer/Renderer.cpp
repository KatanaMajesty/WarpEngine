#include "Renderer.h"

#include "../Util/String.h"
#include "../Core/Logger.h"
#include "../Core/Assert.h"
#include "../Core/Application.h"

// TODO: Temp, remove
#include <DirectXMath.h>

namespace Warp
{

	Renderer::Renderer(HWND hwnd)
		: m_physicalDevice(
			[hwnd]() -> std::unique_ptr<GpuPhysicalDevice>
			{
				GpuPhysicalDeviceDesc physicalDeviceDesc;
				physicalDeviceDesc.hwnd = hwnd;
#ifdef WARP_DEBUG
				physicalDeviceDesc.EnableDebugLayer = true;
				physicalDeviceDesc.EnableGpuBasedValidation = true;
#else
				physicalDeviceDesc.EnableDebugLayer = false;
				physicalDeviceDesc.EnableGpuBasedValidation = false;
#endif
				return std::make_unique<GpuPhysicalDevice>(physicalDeviceDesc);
			}()
		)
		, m_device(m_physicalDevice->GetAssociatedLogicalDevice())
		, m_commandContext(m_device->GetGraphicsQueue())
		, m_swapchain(std::make_unique<RHISwapchain>(m_physicalDevice.get()))
		, m_rootSignature(m_device, RHIRootSignatureDesc(0, 0, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT))
	{

		D3D12_FEATURE_DATA_D3D12_OPTIONS7 feature; 
		m_device->CheckLogicalDeviceFeatureSupport<D3D12_FEATURE_D3D12_OPTIONS7>(feature);
		WARP_ASSERT(feature.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1, "Cannot run with no mesh shader support");

		// TODO: Temp, remove
		if (!InitAssets())
		{
			WARP_LOG_ERROR("Failed to init assets");
			return;
		}

		WARP_LOG_INFO("Renderer was initialized successfully");
	}
	
	Renderer::~Renderer()
	{
		WaitForGfxToFinish();
		m_device->GetComputeQueue()->HostWaitIdle();
		m_device->GetCopyQueue()->HostWaitIdle();
	}

	void Renderer::Resize(uint32_t width, uint32_t height)
	{
		// Wait for all frames to be completed before resizing the backbuffers
		// as they may still be used
		WaitForGfxToFinish();
		m_swapchain->Resize(width, height);
	}

	void Renderer::RenderFrame()
	{
		// Wait for inflight frame of the currently used backbuffer
		UINT frameIndex = m_swapchain->GetCurrentBackbufferIndex();
		WaitForGfxOnFrameToFinish(frameIndex);

		m_device->BeginFrame();
		{
			// Populate command lists
			// TODO: Remove this
			PopulateCommandList();

			m_frameFenceValues[frameIndex] = m_commandContext.Execute(false);
			m_swapchain->Present(true);
		}
		m_device->EndFrame();
	}

	void Renderer::WaitForGfxOnFrameToFinish(uint32_t frameIndex)
	{
		m_device->GetGraphicsQueue()->HostWaitForValue(m_frameFenceValues[frameIndex]);
	}

	void Renderer::WaitForGfxToFinish()
	{
		m_device->GetGraphicsQueue()->HostWaitIdle();
	}

	bool Renderer::InitAssets()
	{
		// TODO: Temporary
		ID3D12Device* d3dDevice = m_device->GetD3D12Device();

		if (!InitShaders())
		{
			WARP_LOG_ERROR("Failed to init shaders");
			return false;
		}

		RHIGraphicsPipelineDesc psoDesc = {};
		psoDesc.RootSignature = m_rootSignature;
		psoDesc.InputElements.push_back({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		psoDesc.InputElements.push_back({ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		psoDesc.VS = m_vs.GetBinaryBytecode();
		psoDesc.PS = m_ps.GetBinaryBytecode();
		psoDesc.DepthStencil.DepthEnable = FALSE;
		psoDesc.DepthStencil.StencilEnable = FALSE;
		psoDesc.NumRTVs = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		m_pso = RHIGraphicsPipelineState(m_device, psoDesc);

		WARP_ASSERT(DirectX::XMVerifyCPUSupport(), "Cannot use DirectXMath for the provided CPU");
		struct Vertex
		{
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT4 color;
		};
		// Define the geometry for a triangle.
		float aspectRatio = static_cast<float>(m_swapchain->GetWidth()) / m_swapchain->GetHeight();
		Vertex triangleVertices[] =
		{
			{ { 0.0f, 0.25f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ { 0.25f, -0.25f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.25f, -0.25f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
		};

		const UINT vertexBufferSize = sizeof(triangleVertices);

		m_vertexBuffer = m_device->CreateBuffer(sizeof(Vertex), vertexBufferSize);
		if (!m_vertexBuffer.IsValid())
		{
			WARP_LOG_FATAL("failed to create vertex buffer");
		}

		Vertex* vertexData = m_vertexBuffer.GetCpuVirtualAddress<Vertex>();
		WARP_ASSERT(vertexData);
		memcpy(vertexData, triangleVertices, vertexBufferSize);

		WaitForGfxToFinish();
		return true;
	}

	bool Renderer::InitShaders()
	{
		std::string filepath = (Application::Get().GetShaderPath() / "Triangle.hlsl").string();
		ShaderCompilationDesc vsShaderDesc = ShaderCompilationDesc("VSMain", EShaderModel::sm_6_5, EShaderType::Vertex);
		ShaderCompilationDesc psShaderDesc = ShaderCompilationDesc("PSMain", EShaderModel::sm_6_5, EShaderType::Pixel);

		m_vs = m_shaderCompiler.CompileShader(filepath, vsShaderDesc, eShaderCompilationFlag_StripDebug | eShaderCompilationFlag_StripReflect);
		m_ps = m_shaderCompiler.CompileShader(filepath, psShaderDesc);
		return m_vs.GetBinaryPointer() && m_ps.GetBinaryPointer();
	}

	void Renderer::PopulateCommandList()
	{
		m_commandContext.Open();
		m_commandContext.SetPipelineState(m_pso);

		UINT width = m_swapchain->GetWidth();
		UINT height = m_swapchain->GetHeight();
		m_commandContext.SetViewport(0, 0, width, height);
		m_commandContext.SetScissorRect(0, 0, width, height);
		m_commandContext.SetGraphicsRootSignature(m_rootSignature);

		// Indicate that the back buffer will be used as a render target.
		UINT currentBackbufferIndex = m_swapchain->GetCurrentBackbufferIndex();
		GpuTexture* backbuffer = m_swapchain->GetBackbuffer(currentBackbufferIndex);
		m_commandContext.AddTransitionBarrier(backbuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D12_CPU_DESCRIPTOR_HANDLE backbufferRtv = m_swapchain->GetRtvDescriptor(currentBackbufferIndex);
		m_commandContext->OMSetRenderTargets(1, &backbufferRtv, FALSE, nullptr);

		auto vbv = m_vertexBuffer.GetVertexBufferView();

		// Record commands.
		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };

		// m_commandList->SetPipelineState(m_pso.Get());
		m_commandContext->ClearRenderTargetView(backbufferRtv, clearColor, 0, nullptr);
		m_commandContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_commandContext->IASetVertexBuffers(0, 1, &vbv);
		m_commandContext.DrawInstanced(3, 1, 0, 0);

		// Indicate that the back buffer will now be used to present.
		m_commandContext.AddTransitionBarrier(backbuffer, D3D12_RESOURCE_STATE_PRESENT);
		m_commandContext.Close();
	}

}