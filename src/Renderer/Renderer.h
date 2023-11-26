#pragma once

#include <vector>
#include <array>

#include "../Core/Defines.h"
#include "RHI/stdafx.h"
#include "RHI/Device.h"
#include "RHI/Descriptor.h"
#include "RHI/DescriptorHeap.h"
#include "RHI/PhysicalDevice.h"
#include "RHI/Resource.h"
#include "RHI/CommandContext.h"
#include "RHI/PipelineState.h"
#include "RHI/Swapchain.h"
#include "RHI/RootSignature.h"
#include "ShaderCompiler.h"

// TODO: Remove these asap
#include "../Assets/ModelAsset.h"

namespace Warp
{

	class Renderer
	{
	public:
		Renderer() = default;
		Renderer(HWND hwnd);

		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		~Renderer();

		void Resize(uint32_t width, uint32_t height);
		
		// TODO: Temporarily takes in ModelAsset
		void RenderFrame(ModelAsset* model);

		// TODO: Maybe temp
		void Update(float timestep);

		static constexpr uint32_t SimultaneousFrames = RHISwapchain::BackbufferCount;
		static constexpr uint32_t NumDescriptorHeapTypes = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
		
		// TODO: Remove these to private to start rewriting bad code
		inline RHIPhysicalDevice* GetPhysicalDevice() const { return m_physicalDevice.get(); }
		inline RHIDevice* GetDevice() const { return m_device.get(); }

		inline RHICommandContext& GetGraphicsContext() { return m_graphicsContext; }
		inline RHICommandContext& GetCopyContext() { return m_copyContext; }
		inline RHICommandContext& GetComputeContext() { return m_computeContext; }

	private:
		// Waits for graphics queue to finish executing on the particular specified frame
		void WaitForGfxOnFrameToFinish(uint32_t frameIndex);

		// Wait for graphics queue to finish executing all the commands on all frames
		void WaitForGfxToFinish();

		std::unique_ptr<RHIPhysicalDevice>	m_physicalDevice;
		std::unique_ptr<RHIDevice>			m_device;
		std::unique_ptr<RHISwapchain>		m_swapchain;

		RHIRootSignature m_rootSignature;
		UINT64 m_frameFenceValues[SimultaneousFrames];

		std::array<std::unique_ptr<RHIDescriptorHeap>, NumDescriptorHeapTypes> m_descriptorHeaps;

		// TODO: Remove/move
		void InitDepthStencil();
		void ResizeDepthStencil();
		std::unique_ptr<RHITexture> m_depthStencil;
		std::unique_ptr<RHIDescriptorHeap> m_dsvHeap;
		RHIDepthStencilView m_depthStencilView;

		RHICommandContext m_graphicsContext;
		RHICommandContext m_copyContext;
		RHICommandContext m_computeContext;

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