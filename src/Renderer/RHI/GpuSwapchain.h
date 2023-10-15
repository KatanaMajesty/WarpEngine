#pragma once

#include "stdafx.h"
#include "GpuResource.h"

namespace Warp
{

	class GpuSwapchain : public GpuDeviceChild
	{
	public:
		GpuSwapchain() = default;
		GpuSwapchain(GpuDevice* device, HWND hwnd);

		GpuSwapchain(const GpuSwapchain&) = default;
		GpuSwapchain& operator=(const GpuSwapchain&) = default;

		GpuSwapchain(GpuSwapchain&&) = default;
		GpuSwapchain& operator=(GpuSwapchain&&) = default;

		static constexpr UINT BackbufferCount = 3;
		static constexpr DXGI_FORMAT BackbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		void Resize(UINT width, UINT height);

		GpuTexture& GetBackbuffer(UINT backbufferIndex) { return m_backbuffers[backbufferIndex]; }
		const GpuTexture& GetBackbuffer(UINT backbufferIndex) const { return m_backbuffers[backbufferIndex]; }

		inline IDXGISwapChain* GetDXGISwapchain() const { return m_handle.Get(); }
		inline IDXGISwapChain4* GetDXGISwapchain4() const { return m_handle.Get(); }

	private:
		ComPtr<IDXGISwapChain4> m_handle;
		GpuTexture m_backbuffers[BackbufferCount];
	};

}