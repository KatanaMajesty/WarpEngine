#include "CommandAllocatorPool.h"

#include "CommandQueue.h"
#include "Device.h"

namespace Warp
{

	// =======================
	// RHICommandAllocatorPool
	// =======================

	RHICommandAllocatorPool::RHICommandAllocatorPool(RHICommandQueue* queue)
		: m_queue(queue)
	{
		WARP_ASSERT(queue);
	}

	ComPtr<ID3D12CommandAllocator> RHICommandAllocatorPool::GetCommandAllocator()
	{
		if (!m_activeAllocators.empty())
		{
			auto [allocator, fenceValue] = m_activeAllocators.front();

			// If we have a free allocator in the queue, we retrieve it from front
			if (m_queue->IsFenceComplete(fenceValue))
			{
				m_activeAllocators.pop();
				WARP_RHI_VALIDATE(allocator->Reset());
				return allocator;
			}
		}

		// Otherwise, we create a new allocator
		ComPtr<ID3D12CommandAllocator> allocator;
		RHIDevice* device = m_queue->GetDevice();
		WARP_RHI_VALIDATE(device->GetD3D12Device()->CreateCommandAllocator(m_queue->GetType(), IID_PPV_ARGS(allocator.GetAddressOf())));
		WARP_RHI_VALIDATE(allocator->Reset());
		return allocator;
	}

	void RHICommandAllocatorPool::DiscardCommandAllocator(ComPtr<ID3D12CommandAllocator> allocator, UINT64 fenceValue)
	{
		if (m_queue->IsFenceComplete(fenceValue))
		{
			return;
		}

		m_activeAllocators.push(std::make_pair(allocator, fenceValue));
	}

}