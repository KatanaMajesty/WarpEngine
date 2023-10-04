#pragma once

#include <vector>
#include "stdafx.h"
#include "../Core/Defines.h"

#include "RHI/GpuCommandQueue.h"
#include "RHI/GpuDevice.h"
#include "RHI/GpuResource.h"

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

		static constexpr uint32_t FrameCount = 2;

	private:
		bool InitD3D12Api(HWND hwnd);

		static inline Renderer* s_instance = nullptr;

		GpuDevice m_device;
		GpuCommandQueue m_commandQueue;

		uint32_t m_width = 0;
		uint32_t m_height = 0;
		ComPtr<IDXGISwapChain3> m_swapchain;
		ComPtr<ID3D12Resource> m_swapchainRtvs[Renderer::FrameCount];
		uint32_t m_rtvDescriptorSize = 0;
		ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		ComPtr<ID3D12DescriptorHeap> m_swapchainRtvDescriptorHeap;
		ComPtr<ID3D12RootSignature> m_rootSignature;

		// TODO: Remove this temp code
		ComPtr<ID3D12PipelineState> m_pso;
		ComPtr<ID3D12GraphicsCommandList> m_commandList;
		ComPtr<ID3DBlob> m_vs;
		ComPtr<ID3DBlob> m_ps;

		GpuBuffer m_vertexBuffer;

		// Synchronization
		uint32_t m_frameIndex = uint32_t(-1);

		bool InitAssets(); // TODO: Temp, will be removed
		bool InitShaders(); // TODO: Temp, will be removed
		void PopulateCommandList(); // TODO: Temp, will be removed
		void WaitForPreviousFrame(uint64_t fenceValue);
	};
}