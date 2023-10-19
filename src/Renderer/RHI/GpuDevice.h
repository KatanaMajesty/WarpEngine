#pragma once

#include <memory>

#include "D3D12MA/D3D12MemAlloc.h"

#include "stdafx.h"
#include "GpuResource.h"
#include "GpuCommandQueue.h"
#include "GpuPhysicalDevice.h"

namespace Warp
{
	
	// Warp uses an approach somewhat similar to Vulkan's. There are two types of device representations: physical and logical.
	// GpuDevice is a logical representation of a GPU device, which is responsible for:
	//	- Command Queue creation
	//	- Performance querying and Profiling
	//	- Querying device's features and retrieving information about the physical device's capabilities
	//	- Frame management
	//
	// TODO: Implement feature querying and adapter infromation querying
	class GpuDevice
	{
	public:
		GpuDevice() = default;
		GpuDevice(GpuPhysicalDevice* physicalDevice);

		// No need to copy logical device, as two different logical devices of the same physical device will in fact
		// be in the same memory, thus making them unnecessary or even unsafe in some cases
		GpuDevice(const GpuDevice&) = delete;
		GpuDevice& operator=(const GpuDevice&) = delete;

		~GpuDevice();

		void BeginFrame();
		void EndFrame();

		WARP_ATTR_NODISCARD inline ID3D12Device9* GetD3D12Device9() const { return m_device.Get(); }
		WARP_ATTR_NODISCARD inline ID3D12Device* GetD3D12Device() const { return GetD3D12Device9(); }

		WARP_ATTR_NODISCARD inline constexpr UINT GetFrameID() const { return m_frameID; }

		WARP_ATTR_NODISCARD GpuCommandQueue* GetQueue(D3D12_COMMAND_LIST_TYPE type) const;
		WARP_ATTR_NODISCARD inline GpuCommandQueue* GetGraphicsQueue() const { return m_graphicsQueue.get(); }
		WARP_ATTR_NODISCARD inline GpuCommandQueue* GetComputeQueue() const { return m_computeQueue.get(); }
		WARP_ATTR_NODISCARD inline GpuCommandQueue* GetCopyQueue() const { return m_copyQueue.get(); }

		// TODO: We should not return by value!
		WARP_ATTR_NODISCARD GpuBuffer CreateBuffer(UINT strideInBytes, UINT64 sizeInBytes, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

		inline D3D12MA::Allocator* GetResourceAllocator() const { return m_resourceAllocator.Get(); }

	private:
		GpuPhysicalDevice* m_physicalDevice;

		ComPtr<ID3D12Device9> m_device;
		ComPtr<ID3D12DebugDevice> m_debugDevice;
		ComPtr<ID3D12InfoQueue1> m_debugInfoQueue;
		DWORD m_messageCallbackCookie = DWORD(-1);

		UINT m_frameID = 0;
		ComPtr<D3D12MA::Allocator> m_resourceAllocator;

		void InitCommandQueues();
		std::unique_ptr<GpuCommandQueue> m_graphicsQueue;
		std::unique_ptr<GpuCommandQueue> m_computeQueue;
		std::unique_ptr<GpuCommandQueue> m_copyQueue;
	};

}