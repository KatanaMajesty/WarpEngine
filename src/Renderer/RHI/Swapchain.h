#pragma once

#include "stdafx.h"
#include "Resource.h"
#include "PhysicalDevice.h"
#include "Descriptor.h"
#include "DescriptorHeap.h"

namespace Warp
{

	// TODO: Sync is needed with GPU
	// TODO: There are cases where the RHIPhysicalDevice represents the adapter that is not controlling the window associaction, thus
	// we need to determine a different adapter, that is in fact able to control the swapchain.
	// This is usually a case for laptops with integrated + discrete GPUs
	class RHISwapchain : public RHIPhysicalDeviceChild
	{
	public:
		RHISwapchain() = default;
		RHISwapchain(RHIPhysicalDevice* physicalDevice);

		RHISwapchain(const RHISwapchain&) = delete;
		RHISwapchain& operator=(const RHISwapchain&) = delete;

		// RHISwapchain(RHISwapchain&&) = default;
		// RHISwapchain& operator=(RHISwapchain&&) = default;

		static constexpr UINT BackbufferCount = 3;
		static constexpr DXGI_FORMAT BackbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		void Present(bool vsync);
		void Resize(UINT width, UINT height);

		WARP_ATTR_NODISCARD inline RHITexture* GetBackbuffer(UINT backbufferIndex) { return &m_backbuffers[backbufferIndex]; }
		WARP_ATTR_NODISCARD inline const RHITexture* GetBackbuffer(UINT backbufferIndex) const { return &m_backbuffers[backbufferIndex]; }
		WARP_ATTR_NODISCARD inline const RHIRenderTargetView& GetRtv(UINT index) const { return m_backbufferRtvs[index]; }
		WARP_ATTR_NODISCARD inline const RHIRenderTargetView& GetCurrentRtv() const { return GetRtv(GetCurrentBackbufferIndex()); }
		inline UINT GetCurrentBackbufferIndex() const { return m_DXGISwapchain->GetCurrentBackBufferIndex(); }

		inline IDXGISwapChain* GetDXGISwapchain() const { return m_DXGISwapchain.Get(); }
		inline IDXGISwapChain4* GetDXGISwapchain4() const { return m_DXGISwapchain.Get(); }
		inline constexpr UINT GetWidth() const { return m_width; }
		inline constexpr UINT GetHeight() const { return m_height; }

	private:
		void InitDXGISwapchain();
		void ReleaseAllDXGIReferences();

		UINT m_width = 0;
		UINT m_height = 0;
		ComPtr<IDXGISwapChain4> m_DXGISwapchain;
		RHITexture m_backbuffers[BackbufferCount];
		RHIDescriptorAllocation m_backbufferRtvsAllocation;
		RHIRenderTargetView m_backbufferRtvs[BackbufferCount];
	};

}