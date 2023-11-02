#include "CommandQueue.h"

#include "../../Core/Defines.h"
#include "../../Core/Assert.h"
#include "../../Core/Logger.h"
#include "Device.h"

namespace Warp
{

	RHICommandQueue::RHICommandQueue(RHIDevice* device, D3D12_COMMAND_LIST_TYPE type)
		: RHIDeviceChild(device)
		, m_queueType(type)
		, m_barrierCommandList(device->GetD3D12Device(), type)
		, m_barrierCommandAllocatorPool(this)
	{
		Reset();
		WARP_ASSERT(device);
		ID3D12Device9* D3D12Device = device->GetD3D12Device();

		WARP_MAYBE_UNUSED HRESULT hr;
		D3D12_COMMAND_QUEUE_DESC desc;
		desc.Type = type;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 0;
		hr = D3D12Device->CreateCommandQueue(&desc, IID_PPV_ARGS(m_handle.ReleaseAndGetAddressOf()));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to create D3D12 command queue for a RHICommandQueue");

		UINT64 FenceInitialValue = 0;
		hr = D3D12Device->CreateFence(FenceInitialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to create a fence for a RHICommandQueue");
	}

	UINT64 RHICommandQueue::Signal()
	{
		UINT64 FenceValue = m_fenceNextValue;
		WARP_MAYBE_UNUSED HRESULT hr = m_handle->Signal(m_fence.Get(), FenceValue);
		WARP_ASSERT(SUCCEEDED(hr), "Failed to signal a fence");
		QueryFenceCompletedValue();
		++m_fenceNextValue;
		return FenceValue;
	}

	void RHICommandQueue::WaitForValue(UINT64 fenceValue)
	{
		if (IsFenceComplete(fenceValue))
		{
			return;
		}

		WARP_MAYBE_UNUSED HRESULT hr = m_handle->Wait(m_fence.Get(), fenceValue);
		WARP_ASSERT(SUCCEEDED(hr), "Failed to wait for fence completion (GPU-sided)");
	}

	void RHICommandQueue::HostWaitForValue(UINT64 fenceValue)
	{
		if (IsFenceComplete(fenceValue))
		{
			return;
		}

		// By providing NULL for HANDLE event we basically just start waiting immediately
		WARP_MAYBE_UNUSED HRESULT hr = m_fence->SetEventOnCompletion(fenceValue, NULL);
		WARP_ASSERT(SUCCEEDED(hr), "Failed to wait for fence completion (CPU-sided)");
	}

	bool RHICommandQueue::IsFenceComplete(UINT64 fenceValue) const
	{
		if (fenceValue <= GetFenceLastCompletedValue())
		{
			return true;
		}

		return fenceValue <= QueryFenceCompletedValue();
	}

	UINT64 RHICommandQueue::ExecuteCommandLists(std::span<RHICommandList* const> commandLists, bool waitForCompletion)
	{
		if (commandLists.empty())
		{
			// We do not want to return here actually, so we just warn about the issue
			// return UINT(-1);
			WARP_LOG_WARN("RHICommandQueue::ExecuteCommandLists() was called with no command lists provided");
		}

		UINT numCommandLists = 0;
		UINT numBarrierCommandLists = 0;
		ID3D12CommandList* D3D12CommandLists[32] = {};
		for (RHICommandList* const list : commandLists)
		{
			// Resolve pending resource barriers
			auto resolvedBarriers = list->ResolvePendingResourceBarriers();
			UINT numBarriers = static_cast<UINT>(resolvedBarriers.size());
			if (numBarriers > 0)
			{
				if (!m_barrierCommandAllocator)
				{
					m_barrierCommandAllocator = m_barrierCommandAllocatorPool.GetCommandAllocator();
					WARP_ASSERT(m_barrierCommandAllocator);
				}

				m_barrierCommandList.Open(m_barrierCommandAllocator.Get());
				m_barrierCommandList->ResourceBarrier(numBarriers, resolvedBarriers.data());
				m_barrierCommandList.Close();

				D3D12CommandLists[numCommandLists++] = m_barrierCommandList.GetD3D12CommandList();
				++numBarrierCommandLists;
			}

			// TODO: Check if command list is not empty and if its closed (maybe)
			D3D12CommandLists[numCommandLists++] = list->GetD3D12CommandList();
		}

		WARP_ASSERT(numCommandLists <= 32, "Max amount of simultaneous command lists is 32 (including barrier command lists)");

		// TODO: Resolve resource barriers
		m_handle->ExecuteCommandLists(numCommandLists, D3D12CommandLists);
		UINT64 fenceValue = Signal();

		// If we resolved any barriers, we need to discard command allocator, as we no longer use it
		if (numBarrierCommandLists > 0)
		{
			m_barrierCommandAllocatorPool.DiscardCommandAllocator(std::exchange(m_barrierCommandAllocator, nullptr), fenceValue);
		}

		if (waitForCompletion)
		{
			HostWaitForValue(fenceValue);
			WARP_ASSERT(IsFenceComplete(fenceValue), "Internal error during fence synchronization");
		}

		return fenceValue;
	}

	UINT64 RHICommandQueue::QueryFenceCompletedValue() const
	{
		m_fenceLastCompletedValue = m_fence->GetCompletedValue();
		return m_fenceLastCompletedValue;
	}

	void RHICommandQueue::Reset(UINT64 fenceInitialValue)
	{
		m_handle.Reset();
		m_fence.Reset();
		m_fenceNextValue = fenceInitialValue + 1;
		m_fenceLastCompletedValue = fenceInitialValue;
	}

}