#include "Renderer.h"

#include "../Util/String.h"
#include "../Core/Logger.h"
#include "../Core/Assert.h"
#include "../Core/Application.h"

// TODO: Temp, remove
#include <DirectXMath.h>

namespace Warp
{

	bool Renderer::Create()
	{
		if (s_instance)
			Renderer::Delete();

		s_instance = new Renderer();
		return true;
	}

	void Renderer::Delete()
	{
		delete s_instance;
		s_instance = nullptr;
	}

	bool Renderer::Init(HWND hwnd, uint32_t width, uint32_t height)
	{
		m_width = width;
		m_height = height;
		if (!InitD3D12Api(hwnd))
		{
			WARP_LOG_ERROR("Failed to init D3D12 Components");
			return false;
		}

		// TODO: Temp, remove
		if (!InitAssets())
		{
			WARP_LOG_ERROR("Failed to init assets");
			return false;
		}

		WARP_LOG_INFO("Renderer was initialized successfully");
		return true;
	}

	void Renderer::Render()
	{
		// Populate command lists
		PopulateCommandList();

		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		WARP_MAYBE_UNUSED HRESULT hr = m_swapchain->Present(1, 0);
		WARP_ASSERT(SUCCEEDED(hr));

		WaitForPreviousFrame();
	}

	bool Renderer::InitD3D12Api(HWND hwnd)
	{
		WARP_ASSERT(hwnd, "Provided HWND is invalid");

		UINT dxgiFactoryFlags = 0;
		WARP_MAYBE_UNUSED HRESULT hr;

		// Init debug interface
#ifdef WARP_DEBUG
		ID3D12Debug* di;
		hr = D3D12GetDebugInterface(IID_PPV_ARGS(&di));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to get D3D12 Debug Interface");

		hr = di->QueryInterface(IID_PPV_ARGS(&m_debugInterface));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to query required D3D12 Debug Interface");

		m_debugInterface->EnableDebugLayer();
		m_debugInterface->SetEnableGPUBasedValidation(TRUE);
		di->Release();

		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

		hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to create DXGI Factory");

		// Selecting an adapter
		if (!SelectBestSuitableDXGIAdapter())
		{
			WARP_LOG_FATAL("Failed to select D3D12 Adapter as no suitable was found");
			return false;
		}
		
		DXGI_ADAPTER_DESC1 adapterDesc;
		m_adapter->GetDesc1(&adapterDesc);

		std::string adapterVendor;
		WStringToString(adapterVendor, adapterDesc.Description);

		// Describe selected adapter to the client
		WARP_LOG_INFO("[Renderer] Successfully selected D3D12 Adapter");
		WARP_LOG_INFO("[Renderer] Adapter Description: {}", adapterVendor);
		WARP_LOG_INFO("[Renderer] Available Dedicated Video Memory: {} bytes", adapterDesc.DedicatedVideoMemory);

		hr = D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to create D3D12 Device");

		// Create command queue
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		hr = m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_commandQueue));
		if (FAILED(hr))
		{
			WARP_LOG_FATAL("Failed to create command queue");
			return false;
		}

		DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
		swapchainDesc.Width = m_width;
		swapchainDesc.Height = m_height;
		swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchainDesc.SampleDesc.Count = 1;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.BufferCount = Renderer::FrameCount;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		// swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapchainFullscreenDesc{};
		swapchainFullscreenDesc.RefreshRate.Numerator = 60;
		swapchainFullscreenDesc.RefreshRate.Denominator = 1;
		swapchainFullscreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swapchainFullscreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		swapchainFullscreenDesc.Windowed = TRUE;

		ComPtr<IDXGISwapChain1> swapchain;
		hr = m_factory->CreateSwapChainForHwnd(m_commandQueue.Get(), 
			hwnd, // handle
			&swapchainDesc,
			nullptr, 
			nullptr, // Restrict content to an output target (NULL if not)
			swapchain.GetAddressOf());
		if (FAILED(hr))
		{
			WARP_LOG_FATAL("Failed to create swapchain");
			return false;
		}

		hr = m_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_PRINT_SCREEN); // No need for print-screen rn
		if (FAILED(hr))
		{
			// Do not return
			WARP_LOG_WARN("Failed to monitor window's message queue. Fullscreen-to-Windowed will not be available via alt+enter");
		}

		hr = swapchain.As(&m_swapchain);
		WARP_ASSERT(SUCCEEDED(hr), "Failed to represent swapchain correctly");
		m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

		// Create descriptor heap
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.NumDescriptors = Renderer::FrameCount;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hr = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_swapchainRtvDescriptorHeap));
		if (FAILED(hr))
		{
			WARP_LOG_FATAL("Failed to create descriptor heap for swapchain's rtvs");
			return false;
		}

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// Create frame resources (a render target view for each frame)
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_swapchainRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		for (uint32_t i = 0; i < Renderer::FrameCount; ++i)
		{
			hr = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_swapchainRtvs[i]));
			WARP_ASSERT(SUCCEEDED(hr));
			m_device->CreateRenderTargetView(m_swapchainRtvs[i].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);
		}

		// Create a command allocator
		hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
		if (FAILED(hr))
		{
			WARP_LOG_FATAL("Failed to create command allocator");
			return false;
		}

		return true;
	}

	bool IsDXGIAdapterSuitable(IDXGIAdapter1* adapter, const DXGI_ADAPTER_DESC1& desc)
	{
		// Don't select render driver, provided by D3D12. We only use physical hardware
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			return false;

		// Passing nullptr as a parameter would just test the adapter for compatibility
		if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
			return false;

		return true;
	}

	bool Renderer::SelectBestSuitableDXGIAdapter()
	{
		WARP_ASSERT(m_factory, "Factory cannot be nullptr at this point");

		SIZE_T largestDedicatedMemory = 0;
		IDXGIAdapter1* adapter;
		for (UINT i = 0;
			(HRESULT)m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; // Implicit cast between different types is fine...
			++i)
		{

			DXGI_ADAPTER_DESC1 adapterDesc;
			adapter->GetDesc1(&adapterDesc);
			
			// If the adapter is suitable, we can compare it with current adapter
			if (IsDXGIAdapterSuitable(adapter, adapterDesc) && largestDedicatedMemory < adapterDesc.DedicatedVideoMemory)
			{
				largestDedicatedMemory = adapterDesc.DedicatedVideoMemory;
				m_adapter.Attach(adapter);
				continue;
			}

			adapter->Release();
			adapter = nullptr;
		}

		return m_adapter != nullptr;
	}

	bool Renderer::InitAssets()
	{
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		WARP_MAYBE_UNUSED HRESULT hr;
		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.GetAddressOf(), error.GetAddressOf());
		WARP_ASSERT(SUCCEEDED(hr), "Failed to serialize root signature");

		hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
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

		hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to create graphics pso");

		hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pso.Get(), IID_PPV_ARGS(&m_commandList));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to create command list");

		// Command lists are created in the recording state, but there is nothing
		// to record yet. The main loop expects it to be closed, so close it now.
		m_commandList->Close();

		WARP_ASSERT(DirectX::XMVerifyCPUSupport(), "Cannot use DirectXMath for the provided CPU");
		struct Vertex
		{
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT4 color;
		};
		// Define the geometry for a triangle.
		float aspectRatio = static_cast<float>(m_width) / m_height;
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

		hr = m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_vertexBuffer));

		if (FAILED(hr))
		{
			WARP_LOG_ERROR("Failed to create committed resource");
			return false;
		}

		void* pVertexData = nullptr;
		CD3DX12_RANGE readRange(0, 0);
		m_vertexBuffer->Map(0, &readRange, &pVertexData);
		memcpy(pVertexData, triangleVertices, vertexBufferSize);
		m_vertexBuffer->Unmap(0, nullptr);

		m_vbv.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vbv.StrideInBytes = sizeof(Vertex);
		m_vbv.SizeInBytes = vertexBufferSize;

		// Create synchronization objects and wait until assets have been uploaded to the GPU.
		hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to create fence");

		m_fenceValue = 1;

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!m_fenceEvent)
		{
			WARP_ASSERT(SUCCEEDED(HRESULT_FROM_WIN32(GetLastError())));
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForPreviousFrame();

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
		// Command list allocators can only be reset when the associated 
		// command lists have finished execution on the GPU; apps should use 
		// fences to determine GPU execution progress.
		WARP_MAYBE_UNUSED HRESULT hr = m_commandAllocator->Reset();
		WARP_ASSERT(SUCCEEDED(hr), "Some command lists are still being executed!");
	
		// However, when ExecuteCommandList() is called on a particular command 
		// list, that command list can then be reset at any time and must be before 
		// re-recording.
		hr = m_commandList->Reset(m_commandAllocator.Get(), m_pso.Get());
		WARP_ASSERT(SUCCEEDED(hr));

		// Set necessary state.
		D3D12_VIEWPORT viewport{};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = (FLOAT)m_width;
		viewport.Height = (FLOAT)m_height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		D3D12_RECT rect{};
		rect.left = 0;
		rect.top = 0;
		rect.right = m_width;
		rect.bottom = m_height;

		m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
		m_commandList->RSSetViewports(1, &viewport);
		m_commandList->RSSetScissorRects(1, &rect);

		// Indicate that the back buffer will be used as a render target.
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_swapchainRtvs[m_frameIndex].Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_commandList->ResourceBarrier(1, &barrier);

		const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_swapchainRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
		m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

		// Record commands.
		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		// m_commandList->SetPipelineState(m_pso.Get());
		m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_commandList->IASetVertexBuffers(0, 1, &m_vbv);
		m_commandList->DrawInstanced(3, 1, 0, 0);

		// Indicate that the back buffer will now be used to present.
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_swapchainRtvs[m_frameIndex].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT);
		m_commandList->ResourceBarrier(1, &barrier);

		hr = m_commandList->Close();
		WARP_ASSERT(SUCCEEDED(hr));
	}

	void Renderer::WaitForPreviousFrame()
	{
		// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
		// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
		// sample illustrates how to use fences for efficient resource usage and to
		// maximize GPU utilization.

		// Signal and increment the fence value.
		const UINT64 fence = m_fenceValue;
		WARP_MAYBE_UNUSED HRESULT hr = m_commandQueue->Signal(m_fence.Get(), fence);
		WARP_ASSERT(SUCCEEDED(hr));

		m_fenceValue++;

		// Wait until the previous frame is finished.
		if (m_fence->GetCompletedValue() < fence)
		{
			hr = m_fence->SetEventOnCompletion(fence, m_fenceEvent);
			WARP_ASSERT(SUCCEEDED(hr));

			WaitForSingleObject(m_fenceEvent, INFINITE);
		}

		m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
	}

}