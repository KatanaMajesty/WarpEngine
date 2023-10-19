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

	class Renderer
	{
	private:
		Renderer() = default;

	public:
		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;
		~Renderer();

		static bool Create();
		static void Delete();
		static inline constexpr Renderer& Get() { return *s_instance; }

		bool Init(HWND hwnd, uint32_t width, uint32_t height);

		// TODO: Temporary, just drawing a triangle
		void Render();

	private:
		bool InitD3D12Api(HWND hwnd);

		static inline Renderer* s_instance = nullptr;

		std::unique_ptr<GpuPhysicalDevice> m_physicalDevice;
		GpuDevice* m_device;
		GpuBuffer m_vertexBuffer;

		uint32_t m_width = 0;
		uint32_t m_height = 0;

		std::unique_ptr<RHISwapchain> m_swapchain;
		ComPtr<ID3D12RootSignature> m_rootSignature;

		// TODO: Remove this temp code
		RHICommandContext m_commandContext;
		ComPtr<ID3D12PipelineState> m_pso;
		ComPtr<ID3DBlob> m_vs;
		ComPtr<ID3DBlob> m_ps;

		bool InitAssets(); // TODO: Temp, will be removed
		bool InitShaders(); // TODO: Temp, will be removed
		void PopulateCommandList(); // TODO: Temp, will be removed
		void WaitForPreviousFrame(uint64_t fenceValue);
	};
}