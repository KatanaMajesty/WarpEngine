#include "Swapchain.h"

#include "../../Core/Assert.h"
#include "Device.h"

namespace Warp
{

	RHISwapchain::RHISwapchain(RHIPhysicalDevice* physicalDevice)
		: RHIPhysicalDeviceChild(physicalDevice)
		, m_rtvHeap(physicalDevice->GetAssociatedLogicalDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, BackbufferCount, false)
	{
		InitDXGISwapchain();

		DXGI_SWAP_CHAIN_DESC1 desc;
		WARP_RHI_VALIDATE(m_DXGISwapchain->GetDesc1(&desc));
		Resize(desc.Width, desc.Height);

	}

	void RHISwapchain::Present(bool vsync)
	{
		// https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/d3d10-graphics-programming-guide-dxgi
		// TODO: Start using dirty rectangles - https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-1-2-presentation-improvements
		// UINT SwapchainFlags = DXGI_PRESENT_ALLOW_TEARING;
		UINT syncInterval = vsync ? 1 : 0;
		m_DXGISwapchain->Present(syncInterval, 0);
	}

	void RHISwapchain::Resize(UINT width, UINT height)
	{
		if (width == m_width && height == m_height)
		{
			WARP_ASSERT(width != 0 && height != 0);
			return;
		}

		m_width = width;
		m_height = height;

		ReleaseAllDXGIReferences();
		UINT swapchainFlags = 0; // No swapchain flags for now. Be aware of this when they are added tho
		WARP_RHI_VALIDATE(m_DXGISwapchain->ResizeBuffers(BackbufferCount, width, height, DXGI_FORMAT_UNKNOWN, swapchainFlags));

		// We assume that the client already issued a wait for graphics queue to finish executing all frames
		RHIDevice* device = GetPhysicalDevice()->GetAssociatedLogicalDevice();
		for (uint32_t i = 0; i < BackbufferCount; ++i)
		{
			ComPtr<ID3D12Resource> buffer;
			WARP_RHI_VALIDATE(m_DXGISwapchain->GetBuffer(i, IID_PPV_ARGS(buffer.GetAddressOf())));

			m_backbuffers[i] = RHITexture(device, buffer.Get(), D3D12_RESOURCE_STATE_PRESENT);

			device->GetD3D12Device()->CreateRenderTargetView(
				m_backbuffers[i].GetD3D12Resource(), 
				nullptr, 
				m_backbufferRtvs.GetCpuAddress(i));
		}
	}

	void RHISwapchain::InitDXGISwapchain()
	{
		IDXGIFactory7* factory = m_physicalDevice->GetFactory();
		ID3D12CommandQueue* queue = m_physicalDevice->GetAssociatedLogicalDevice()->GetGraphicsQueue()->GetInternalHandle();

		DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
		swapchainDesc.Width = m_width;
		swapchainDesc.Height = m_height;
		swapchainDesc.Format = BackbufferFormat;
		swapchainDesc.SampleDesc.Count = 1;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.BufferCount = BackbufferCount;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		// swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapchainFullscreenDesc{};
		swapchainFullscreenDesc.RefreshRate.Numerator = 60;
		swapchainFullscreenDesc.RefreshRate.Denominator = 1;
		swapchainFullscreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swapchainFullscreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		swapchainFullscreenDesc.Windowed = TRUE;

		ComPtr<IDXGISwapChain1> swapchain1;
		factory->CreateSwapChainForHwnd(queue, m_physicalDevice->GetWindowHandle(),
			&swapchainDesc,
			&swapchainFullscreenDesc,
			nullptr,
			swapchain1.GetAddressOf());

		WARP_MAYBE_UNUSED HRESULT hr = swapchain1.As(&m_DXGISwapchain);
		WARP_ASSERT(SUCCEEDED(hr), "Failed to represent swapchain correctly");

		m_backbufferRtvs = m_rtvHeap.Allocate(BackbufferCount);
	}

	void RHISwapchain::ReleaseAllDXGIReferences()
	{
		// No need to reallocate descriptors for now
		// WARP_ASSERT(m_backbufferRtvs.IsValid());
		// m_rtvHeap.Free(std::move(m_backbufferRtvs));

		for (uint32_t i = 0; i < BackbufferCount; ++i)
			m_backbuffers[i] = RHITexture();
	}

}