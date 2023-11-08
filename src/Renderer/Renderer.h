#pragma once

#include <vector>

#include "../Core/Defines.h"
#include "RHI/stdafx.h"
#include "RHI/Device.h"
#include "RHI/PhysicalDevice.h"
#include "RHI/Resource.h"
#include "RHI/CommandContext.h"
#include "RHI/PipelineState.h"
#include "RHI/DescriptorHeap.h"
#include "RHI/Swapchain.h"
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

		std::unique_ptr<RHIPhysicalDevice> m_physicalDevice;
		RHIDevice* m_device;

		std::unique_ptr<RHISwapchain> m_swapchain;
		RHICommandContext m_commandContext; // TODO: Make it unique_ptr after we implement whole functionality
		RHIRootSignature m_rootSignature;
		UINT64 m_frameFenceValues[SimultaneousFrames];

		// TODO: Remove/move
		void InitDepthStencil();
		void ResizeDepthStencil();
		std::unique_ptr<RHITexture> m_depthStencil;
		std::unique_ptr<RHIDescriptorHeap> m_dsvHeap;
		RHIDescriptorAllocation m_dsv;

		RHIBuffer m_constantBuffer;
		CShaderCompiler m_shaderCompiler;

		float m_timeElapsed = 0.0f;
		RHIMeshPipelineState m_cubePso;
		CShader m_cubeMs;
		CShader m_cubePs;

		bool InitAssets(); // TODO: Temp, will be removed
		bool InitShaders(); // TODO: Temp, will be removed
	};
}