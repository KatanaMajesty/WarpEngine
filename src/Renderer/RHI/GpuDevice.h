#pragma once

#include <memory>

#include "D3D12MA/D3D12MemAlloc.h"

#include "../stdafx.h"
#include "GpuResource.h"

namespace Warp
{

	struct GpuDeviceDesc
	{
		HWND hwnd = nullptr;

		// Enables debug layer for a GPU device. This allows to enable.
		// The debug layer adds important debug and diagnostic features for application developers during application development.
		bool EnableDebugLayer = false;

		// GPU-based validation helps to identify the following errors :
		// 
		//	- Use of uninitialized or incompatible descriptors in a shader.
		//	- Use of descriptors referencing deleted Resources in a shader.
		//	- Validation of promoted resource states and resource state decay.
		//	- Indexing beyond the end of the descriptor heap in a shader.
		//	- Shader accesses of resources in incompatible state.
		//	- Use of uninitialized or incompatible Samplers in a shader.
		// 
		// For more info see https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer-gpu-based-validation
		// NOTICE! GPU-based validation can only be enabled if the debug layer is enabled as well.
		bool EnableGpuBasedValidation = false;
	};

	class GpuDevice
	{
	public:
		GpuDevice() = default;
		~GpuDevice();

		bool Init(const GpuDeviceDesc& desc);

		void BeginFrame();
		void EndFrame();

		inline IDXGIFactory7* GetFactory() const { return m_factory.Get(); }
		inline IDXGIAdapter1* GetAdapter() const { return m_adapter.Get(); }
		inline ID3D12Device9* GetD3D12Device9() const { return m_device.Get(); }
		inline ID3D12Device* GetD3D12Device() const { return GetD3D12Device9(); }

		inline bool IsDebugLayerEnabled() const { return m_debugInterface != nullptr; }

		inline constexpr UINT GetFrameID() const { return m_frameID; }
		inline constexpr UINT GetNumSimultaneousFrames() const { return m_numSimultaneousFrames; }

		GpuBuffer CreateBuffer(UINT strideInBytes, UINT64 sizeInBytes, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	private:
		bool SelectBestSuitableDXGIAdapter();

		ComPtr<IDXGIFactory7> m_factory;
		ComPtr<IDXGIAdapter1> m_adapter;
		ComPtr<ID3D12Device9> m_device;

		ComPtr<ID3D12Debug3> m_debugInterface;
		ComPtr<ID3D12InfoQueue1> m_debugInfoQueue;
		DWORD m_messageCallbackCookie = DWORD(-1);

		UINT m_frameID = 0;
		UINT m_numSimultaneousFrames = 3;
		ComPtr<D3D12MA::Allocator> m_resourceAllocator = nullptr;
	};

}