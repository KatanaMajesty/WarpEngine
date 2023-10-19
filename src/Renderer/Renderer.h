#pragma once

#include <vector>

#include "../Core/Defines.h"
#include "RHI/stdafx.h"
#include "RHI/GpuDevice.h"
#include "RHI/GpuPhysicalDevice.h"
#include "RHI/GpuResource.h"
#include "RHI/RHICommandContext.h"
#include "RHI/GpuDescriptorHeap.h"
#include "RHI/RHISwapchain.h"

namespace Warp
{

	// TODO: Remove singleton, its annoying
	class Renderer
	{
	public:
		Renderer() = default;
		Renderer(HWND hwnd);

		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		void Resize(uint32_t width, uint32_t height);
		void RenderFrame();

		static constexpr uint32_t SimultaneousFrames = RHISwapchain::BackbufferCount;

	private:
		// Waits for graphics queue to finish executing on the particular specified frame
		void WaitForGfxOnFrameToFinish(uint32_t frameIndex);

		// Wait for graphics queue to finish executing all the commands on all frames
		void WaitForGfxToFinish();

		std::unique_ptr<GpuPhysicalDevice> m_physicalDevice;
		GpuDevice* m_device;

		std::unique_ptr<RHISwapchain> m_swapchain;
		RHICommandContext m_commandContext; // TODO: Make it unique_ptr after we implement whole functionality
		UINT64 m_frameFenceValues[SimultaneousFrames];

		GpuBuffer m_vertexBuffer;

		ComPtr<ID3D12RootSignature> m_rootSignature;

		// TODO: Remove this temp code
		ComPtr<ID3D12PipelineState> m_pso;
		ComPtr<ID3DBlob> m_vs;
		ComPtr<ID3DBlob> m_ps;

		bool InitAssets(); // TODO: Temp, will be removed
		bool InitShaders(); // TODO: Temp, will be removed
		void PopulateCommandList(); // TODO: Temp, will be removed
		void WaitForPreviousFrame(uint64_t fenceValue);
	};
}