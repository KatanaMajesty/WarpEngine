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
#endif
				return std::make_unique<GpuPhysicalDevice>(physicalDeviceDesc);
			}()
		)
		, m_device(m_physicalDevice->GetAssociatedLogicalDevice())
		, m_commandContext(m_device->GetGraphicsQueue())
		, m_swapchain(std::make_unique<RHISwapchain>(m_physicalDevice.get()))
	{
		// TODO: Temp, remove
		if (!InitAssets())
		{
			WARP_LOG_ERROR("Failed to init assets");
			return;
		}

		WARP_LOG_INFO("Renderer was initialized successfully");
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

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		HRESULT hr;
		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.GetAddressOf(), error.GetAddressOf());
		WARP_ASSERT(SUCCEEDED(hr), "Failed to serialize root signature");

		hr = d3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to create root signature");

		if (!InitShaders())
		{
			WARP_LOG_ERROR("Failed to init shaders");
			return false;
		}

		D3D12_INPUT_ELEMENT_DESC elementDescs[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vs.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_ps.Get());
		// psoDesc.DS;
		// psoDesc.HS;
		// psoDesc.GS;
		// D3D12_STREAM_OUTPUT_DESC StreamOutput;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = false;
		psoDesc.DepthStencilState.StencilEnable = false;
		psoDesc.InputLayout = D3D12_INPUT_LAYOUT_DESC{ elementDescs, 2 };
		// D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		// DXGI_FORMAT DSVFormat;
		psoDesc.SampleDesc.Count = 1;
		// UINT NodeMask;
		// D3D12_CACHED_PIPELINE_STATE CachedPSO;
		// D3D12_PIPELINE_STATE_FLAGS Flags;

		hr = d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to create graphics pso");

		// hr = d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pso.Get(), IID_PPV_ARGS(&m_commandList));
		// WARP_ASSERT(SUCCEEDED(hr), "Failed to create command list");

		// Command lists are created in the recording state, but there is nothing
		// to record yet. The main loop expects it to be closed, so close it now.
		// m_commandList->Close();

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
			{ { 0.0f, 0.25f * aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ { 0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
		};

		const UINT vertexBufferSize = sizeof(triangleVertices);

		// Note: using upload heaps to transfer static data like vert buffers is not 
		// recommended. Every time the GPU needs it, the upload heap will be marshalled 
		// over. Please read up on Default Heap usage. An upload heap is used here for 
		// code simplicity and because there are very few verts to actually transfer.
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

		m_vertexBuffer = m_device->CreateBuffer(sizeof(Vertex), vertexBufferSize);
		if (!m_vertexBuffer.IsValid())
		{
			WARP_LOG_FATAL("failed to create vertex buffer");
		}

		if (FAILED(hr))
		{
			WARP_LOG_ERROR("Failed to create committed resource");
			return false;
		}

		Vertex* vertexData = m_vertexBuffer.GetCpuVirtualAddress<Vertex>();
		memcpy(vertexData, triangleVertices, vertexBufferSize);

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		GpuCommandQueue* queue = m_device->GetGraphicsQueue();
		queue->HostWaitForValue(queue->Signal());

		return true;
	}

	bool Renderer::InitShaders()
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;
		UINT compileFlags = 0;

#ifdef WARP_DEBUG
		compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

		std::string filepath = (Application::Get().GetShaderPath() / "Triangle.hlsl").string();
		std::wstring wfilepath(filepath.begin(), filepath.end());
		WARP_MAYBE_UNUSED HRESULT hr;
		hr = D3DCompileFromFile(wfilepath.c_str(), nullptr, nullptr, "vs_main", "vs_5_0", compileFlags, 0, m_vs.ReleaseAndGetAddressOf(), nullptr);
		WARP_ASSERT(SUCCEEDED(hr), "Failed to compile vs");

		hr = D3DCompileFromFile(wfilepath.c_str(), nullptr, nullptr, "ps_main", "ps_5_0", compileFlags, 0, m_ps.ReleaseAndGetAddressOf(), nullptr);
		WARP_ASSERT(SUCCEEDED(hr), "Failed to compile ps");

		return true;
	}

	void Renderer::PopulateCommandList()
	{
		m_commandContext.Open();
		m_commandContext->SetPipelineState(m_pso.Get());

		UINT width = m_swapchain->GetWidth();
		UINT height = m_swapchain->GetHeight();
		// Set necessary state.
		D3D12_VIEWPORT viewport{};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = (FLOAT)width;
		viewport.Height = (FLOAT)height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		D3D12_RECT rect{};
		rect.left = 0;
		rect.top = 0;
		rect.right = width;
		rect.bottom = height;

		m_commandContext->SetGraphicsRootSignature(m_rootSignature.Get());
		m_commandContext->RSSetViewports(1, &viewport);
		m_commandContext->RSSetScissorRects(1, &rect);

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

	void Renderer::WaitForPreviousFrame(uint64_t fenceValue)
	{
		// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
		// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
		// sample illustrates how to use fences for efficient resource usage and to
		// maximize GPU utilization.

		// Signal and increment the fence value.
		// m_device->GetGraphicsQueue()->HostWaitForValue(fenceValue);
	}

}