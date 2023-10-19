#pragma once

#include "stdafx.h"
#include "GpuResource.h"
#include "GpuPhysicalDevice.h"
#include "GpuDescriptorHeap.h"

namespace Warp
{

	// TODO: Sync is needed with GPU
	// TODO: There are cases where the GpuPhysicalDevice represents the adapter that is not controlling the window associaction, thus
	// we need to determine a different adapter, that is in fact able to control the swapchain.
	// This is usually a case for laptops with integrated + discrete GPUs
	class RHISwapchain : public GpuPhysicalDeviceChild
	{
	public:
		RHISwapchain() = default;
		RHISwapchain(GpuPhysicalDevice* physicalDevice);

		RHISwapchain(const RHISwapchain&) = delete;
		RHISwapchain& operator=(const RHISwapchain&) = delete;

		// RHISwapchain(RHISwapchain&&) = default;
		// RHISwapchain& operator=(RHISwapchain&&) = default;

		static constexpr UINT BackbufferCount = 3;
		static constexpr DXGI_FORMAT BackbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		void Present(bool vsync);
		void Resize(UINT width, UINT height);

		WARP_ATTR_NODISCARD inline GpuTexture* GetBackbuffer(UINT backbufferIndex) { return &m_backbuffers[backbufferIndex]; }
		WARP_ATTR_NODISCARD inline const GpuTexture* GetBackbuffer(UINT backbufferIndex) const { return &m_backbuffers[backbufferIndex]; }
		WARP_ATTR_NODISCARD inline D3D12_CPU_DESCRIPTOR_HANDLE GetRtvDescriptor(UINT index) const { return m_backbufferRtvs.GetCpuAddress(index); }
		inline UINT GetCurrentBackbufferIndex() const { return m_DXGISwapchain->GetCurrentBackBufferIndex(); }

		inline IDXGISwapChain* GetDXGISwapchain() const { return m_DXGISwapchain.Get(); }
		inline IDXGISwapChain4* GetDXGISwapchain4() const { return m_DXGISwapchain.Get(); }
		inline constexpr UINT GetWidth() const { return m_width; }
		inline constexpr UINT GetHeight() const { return m_height; }

	private:
		void InitDXGISwapchain();

		UINT m_width = 0;
		UINT m_height = 0;
		ComPtr<IDXGISwapChain4> m_DXGISwapchain;
		GpuTexture m_backbuffers[BackbufferCount];
		
		// TODO: For now only it is just a handle
		GpuDescriptorHeap m_rtvHeap;
		RHIDescriptorAllocation m_backbufferRtvs{};
	};

}