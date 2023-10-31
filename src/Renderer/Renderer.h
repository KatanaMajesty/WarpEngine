#pragma once

#include <vector>

#include "../Core/Defines.h"
#include "RHI/stdafx.h"
#include "RHI/GpuDevice.h"
#include "RHI/GpuPhysicalDevice.h"
#include "RHI/GpuResource.h"
#include "RHI/RHICommandContext.h"
#include "RHI/GpuPipelineState.h"
#include "RHI/GpuDescriptorHeap.h"
#include "RHI/RHISwapchain.h"
#include "RHI/RootSignature.h"
#include "ShaderCompiler.h"

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

		~Renderer();

		void Resize(uint32_t width, uint32_t height);
		void RenderFrame();

		// TODO: Maybe temp
		void Update(float timestep);

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
		RHIRootSignature m_rootSignature;
		UINT64 m_frameFenceValues[SimultaneousFrames];

		// TODO: Remove/move
		void InitDepthStencil();
		std::unique_ptr<GpuTexture> m_depthStencil;
		std::unique_ptr<GpuDescriptorHeap> m_dsvHeap;
		RHIDescriptorAllocation m_dsv;

		GpuBuffer m_constantBuffer;
		CShaderCompiler m_shaderCompiler;

		float m_timeElapsed = 0.0f;
		RHIMeshPipelineState m_cubePso;
		CShader m_cubeMs;
		CShader m_cubePs;

		bool InitAssets(); // TODO: Temp, will be removed
		bool InitShaders(); // TODO: Temp, will be removed
		void PopulateCommandList(); // TODO: Temp, will be removed
	};
}