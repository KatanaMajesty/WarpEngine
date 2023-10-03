#include "GpuCommandQueue.h"

#include "../../Core/Defines.h"
#include "../../Core/Assert.h"
#include "../../Core/Logger.h"

namespace Warp
{

	bool GpuCommandQueue::Init(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type)
	{
		Reset();

		WARP_MAYBE_UNUSED HRESULT hr;
		D3D12_COMMAND_QUEUE_DESC desc;
		desc.Type = type;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 0;
		hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(m_handle.ReleaseAndGetAddressOf()));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to create D3D12 command queue for a GpuCommandQueue");

		UINT64 FenceInitialValue = 0;
		hr = device->CreateFence(FenceInitialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to create a fence for a GpuCommandQueue");
		return true;
	}

	UINT64 GpuCommandQueue::Signal()
	{
		UINT64 FenceValue = m_fenceNextValue;
		WARP_MAYBE_UNUSED HRESULT hr = m_handle->Signal(m_fence.Get(), FenceValue);
		WARP_ASSERT(SUCCEEDED(hr), "Failed to signal a fence");
		QueryFenceCompletedValue();
		++m_fenceNextValue;
		return FenceValue;
	}

	void GpuCommandQueue::WaitForValue(UINT64 fenceValue)
	{
		if (IsFenceComplete(fenceValue))
		{
			return;
		}

		WARP_MAYBE_UNUSED HRESULT hr = m_handle->Wait(m_fence.Get(), fenceValue);
		WARP_ASSERT(SUCCEEDED(hr), "Failed to wait for fence completion (GPU-sided)");
	}

	void GpuCommandQueue::HostWaitForValue(UINT64 fenceValue)
	{
		if (IsFenceComplete(fenceValue))
		{
			return;
		}

		// By providing NULL for HANDLE event we basically just start waiting immediately
		WARP_MAYBE_UNUSED HRESULT hr = m_fence->SetEventOnCompletion(fenceValue, NULL);
		WARP_ASSERT(SUCCEEDED(hr), "Failed to wait for fence completion (CPU-sided)");
	}

	bool GpuCommandQueue::IsFenceComplete(UINT64 fenceValue) const
	{
		if (fenceValue <= GetFenceLastCompletedValue())
		{
			return true;
		}

		return fenceValue <= QueryFenceCompletedValue();
	}

	UINT64 GpuCommandQueue::ExecuteCommandLists(std::span<ID3D12CommandList*> commandLists, bool waitForCompletion)
	{
		if (commandLists.empty())
		{
			// We do not want to return here actually, so we just warn about the issue
			// return UINT(-1);
			WARP_LOG_WARN("GpuCommandQueue::ExecuteCommandLists() was called with no command lists provided");
		}

		UINT NumCommandLists = static_cast<UINT>(commandLists.size());
		m_handle->ExecuteCommandLists(NumCommandLists, commandLists.data());
		UINT64 FenceValue = Signal();

		if (waitForCompletion)
		{
			HostWaitForValue(FenceValue);
			WARP_ASSERT(IsFenceComplete(FenceValue), "Internal error during fence synchronization");
		}

		return FenceValue;
	}

	UINT64 GpuCommandQueue::QueryFenceCompletedValue() const
	{
		m_fenceLastCompletedValue = m_fence->GetCompletedValue();
		return m_fenceLastCompletedValue;
	}

	void GpuCommandQueue::Reset(UINT64 fenceInitialValue)
	{
		m_handle.Reset();
		m_queueType = D3D12_COMMAND_LIST_TYPE_DIRECT; // Just a default val

		m_fence.Reset();
		m_fenceNextValue = fenceInitialValue + 1;
		m_fenceLastCompletedValue = fenceInitialValue;
	}

}