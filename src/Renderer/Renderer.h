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
#include "../Math/Math.h"

namespace Warp
{

	class World;

	class Renderer
	{
	public:
		Renderer() = default;
		Renderer(HWND hwnd);

		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		~Renderer();

		void Resize(uint32_t width, uint32_t height);
		void Render(World* world);

		static constexpr uint32_t SimultaneousFrames = RHISwapchain::BackbufferCount;
		static constexpr uint32_t NumDescriptorHeapTypes = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
		
		// TODO: Remove these to private to start rewriting bad code
		inline RHIPhysicalDevice* GetPhysicalDevice() const { return m_physicalDevice.get(); }
		inline RHIDevice* GetDevice() const { return m_device.get(); }

		inline RHICommandContext& GetGraphicsContext() { return m_graphicsContext; }
		inline RHICommandContext& GetCopyContext() { return m_copyContext; }
		inline RHICommandContext& GetComputeContext() { return m_computeContext; }

		// Utility function that manages copy context, creates intermediate resource and calls Context::UploadSubresources
		void UploadSubresources(RHIResource* dest, std::vector<D3D12_SUBRESOURCE_DATA>& subresources, uint32_t subresourceOffset);

	private:
		// Waits for graphics queue to finish executing on the particular specified frame or on all frames
		void WaitForGfxOnFrameToFinish(uint32_t frameIndex);
		void WaitForGfxToFinish();

		std::unique_ptr<RHIPhysicalDevice> m_physicalDevice;
		std::unique_ptr<RHIDevice> m_device;
		std::unique_ptr<RHISwapchain> m_swapchain;

		UINT64 m_frameFenceValues[SimultaneousFrames];
		RHICommandContext m_graphicsContext;
		RHICommandContext m_copyContext;
		RHICommandContext m_computeContext;

		// TODO: Remove/move
		void InitDepthStencil();
		void ResizeDepthStencil();
		bool InitAssets(); // TODO: Temp, will be removed
		
		std::unique_ptr<RHITexture> m_depthStencil;
		RHIDepthStencilView m_depthStencilView;
		RHIRootSignature m_basicRootSignature;
		RHIMeshPipelineState m_basicPSO;
		CShaderCompiler m_shaderCompiler;
		CShader m_MSBasic;
		CShader m_PSBasic;
		
		static constexpr size_t SizeOfGlobalCb = 64 * 256;
		RHIBuffer m_constantBuffers[SimultaneousFrames];

		struct UploadResourceTrackedState
		{
			RHIBuffer UploadBuffer;
			UINT64 FenceValue;
		};
		std::vector<UploadResourceTrackedState> m_uploadResourceTrackedStates;
	};
}