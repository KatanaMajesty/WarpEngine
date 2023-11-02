#pragma once

#include <span>

#include "stdafx.h"
#include "../../Core/Defines.h"
#include "CommandList.h"
#include "CommandAllocatorPool.h"
#include "DeviceChild.h"

namespace Warp
{

	class RHICommandQueue : public RHIDeviceChild
	{
	public:
		RHICommandQueue() = default;
		RHICommandQueue(RHIDevice* device, D3D12_COMMAND_LIST_TYPE type);

		RHICommandQueue(const RHICommandQueue&) = default;
		RHICommandQueue& operator=(const RHICommandQueue&) = default;

		RHICommandQueue(RHICommandQueue&&) = default;
		RHICommandQueue& operator=(RHICommandQueue&&) = default;

		// Returns a pointer to the internally handled D3D12 command queue
		inline ID3D12CommandQueue* GetInternalHandle() const { return m_handle.Get(); }
		inline bool IsValid() const { return GetInternalHandle() != nullptr; }

		// Returns a type of the command queue, which is also the only command list type supported by the queue
		inline constexpr D3D12_COMMAND_LIST_TYPE GetType() const { return m_queueType; }

		// Signals internal GpuFence with a fenceNextValue
		UINT64 Signal();

		// GPU-Sided wait for provided fence value completion, meaning that the call to this function returns immediately on CPU
		// and inserts a wait command to the GPU-queue
		void WaitForValue(UINT64 fenceValue);

		// CPU-Sided wait for provided fence value completion
		void HostWaitForValue(UINT64 fenceValue);
		void HostWaitIdle() { HostWaitForValue(Signal()); }

		bool IsFenceComplete(UINT64 fenceValue) const;
		inline constexpr UINT64 GetFenceNextValue() const { return m_fenceNextValue; }
		inline constexpr UINT64 GetFenceLastCompletedValue() const { return m_fenceLastCompletedValue; }

		// Executes command lists, provided as a span. Also signals a fence and returns a fence value
		// If waitForCompletion is TRUE, then the host will wait for the completion of the command list
		// Otherwise, the host can manually wait for the completion using the WaitForValue() or HostWaitForValue() methods providing
		// the returned fence value
		UINT64 ExecuteCommandLists(std::span<RHICommandList* const> commandLists, bool waitForCompletion);

		// Updates last completed value by the fence and stores it, effectively caching it, in m_fenceLastCompletedValue field
		// This value can then be obtained by calling GetFenceLastCompletedValue method
		// 
		// This method is a const-method because cached last fence completed value does not affect the state of command queue in general
		// so it is safe to change it in constant context
		UINT64 QueryFenceCompletedValue() const;

	private:
		void Reset(UINT64 fenceInitialValue = 0);

		ComPtr<ID3D12CommandQueue> m_handle;
		D3D12_COMMAND_LIST_TYPE m_queueType;

		ComPtr<ID3D12Fence> m_fence;
		UINT64 m_fenceNextValue;
		mutable UINT64 m_fenceLastCompletedValue; // We cache last completed value of fence

		RHICommandList m_barrierCommandList;
		RHICommandAllocatorPool m_barrierCommandAllocatorPool;
		ComPtr<ID3D12CommandAllocator> m_barrierCommandAllocator;
	};
}