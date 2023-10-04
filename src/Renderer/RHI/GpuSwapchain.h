#pragma once

#include "../stdafx.h"
#include "GpuResource.h"

namespace Warp
{

	class GpuSwapchain
	{
	public:
		GpuSwapchain() = default;

		bool Init(UINT bufferCount, UINT width, UINT height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

		inline IDXGISwapChain* GetDXGISwapchain() const { return m_handle.Get(); }
		inline IDXGISwapChain4* GetDXGISwapchain4() const { return m_handle.Get(); }

	private:
		ComPtr<IDXGISwapChain4> m_handle;
		GpuTexture m_backbuffer
	};

}