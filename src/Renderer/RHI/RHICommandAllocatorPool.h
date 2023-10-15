#pragma once

#include <queue>
#include "stdafx.h"

namespace Warp
{

	class GpuCommandQueue;

	// We use CommandAllocator pool to retrieve valid command allocator, that can be used immediately after request
	// AllocatorEntry contains command allocator itself and a FenceValue, associated with the command list, that has been executed
	// on with this command allocator
	//
	// FenceValue can be used to check if command allocator is still used in command queue
	class RHICommandAllocatorPool
	{
	public:
		RHICommandAllocatorPool() = default;
		RHICommandAllocatorPool(GpuCommandQueue* queue);

		ComPtr<ID3D12CommandAllocator> GetCommandAllocator();
		void DiscardCommandAllocator(ComPtr<ID3D12CommandAllocator> allocator, UINT64 fenceValue);

	private:
		using AllocatorEntry = std::pair<ComPtr<ID3D12CommandAllocator>, UINT64>;

		GpuCommandQueue* m_queue;
		std::queue<AllocatorEntry> m_activeAllocators;
	};

}