#pragma once

#include <array>

#include "stdafx.h"
#include "GpuResource.h"
#include "../../Core/Defines.h"

namespace Warp
{

	struct RHIResourceStateTracker
	{
		struct PendingResourceBarrier
		{
			GpuResource* Resource;
			UINT SubresourceIndex;
			D3D12_RESOURCE_STATES State;
		};

		RHIResourceStateTracker() = default;

		// Adds a pending resource barrier object to the pending resource barrier queue
		// The pending resource barrier queue will be resolved before executing the command list on the GPU
		// This is needed to resolve and keep track of valid states of unknown resources when they are first used in the command list
		// For more info watch https://www.youtube.com/watch?v=nmB2XMasz2o
		void AddPendingBarrier(const PendingResourceBarrier& barrier);
		void Reset();

		// Queries the cached resource state of a provided resource, if it is not null.
		// If the state of the resource has not been yet cached, it will be added to the cache in UNKNOWN state
		// 
		// The command list is then able to change the cached resource state returned by the function
		GpuResourceState& GetCachedState(GpuResource* resource);

		inline constexpr const auto& GetAllPendingBarriers() const { return m_pendingResourceBarriers; }

	private:
		std::unordered_map<GpuResource*, GpuResourceState> m_cachedResourceStates;
		std::vector<PendingResourceBarrier> m_pendingResourceBarriers;
	};

	class GpuCommandList
	{
	public:
		GpuCommandList() = default;
		GpuCommandList(ID3D12Device9* device, D3D12_COMMAND_LIST_TYPE type);

		inline constexpr D3D12_COMMAND_LIST_TYPE GetType() const { return m_type; }
		inline ID3D12GraphicsCommandList6* GetD3D12CommandList() const { return m_commandList.Get(); }
		inline ID3D12GraphicsCommandList6* operator->() const { return GetD3D12CommandList(); } // TODO: Will be removed

		void AddTransitionBarrier(GpuResource* resource, D3D12_RESOURCE_STATES state, UINT subresourceIndex = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		void AddAliasingBarrier(GpuResource* before, GpuResource* after); // NOIMPL
		void AddUavBarrier(GpuResource* resource); // NOIMPL

		void FlushBatchedResourceBarriers();
		WARP_ATTR_NODISCARD std::vector<D3D12_RESOURCE_BARRIER> ResolvePendingResourceBarriers();

		void Open(ID3D12CommandAllocator* allocator);
		void Close();

	private:
		void AddResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier);
		
		static constexpr UINT NumResourceBarriersPerBatch = 16;

		D3D12_COMMAND_LIST_TYPE m_type;
		ComPtr<ID3D12GraphicsCommandList6> m_commandList;
		RHIResourceStateTracker m_stateTracker;

		// For improved performance, applications should use split barriers (refer to Multi-engine synchronization). 
		// Application should also batch multiple transitions into a single call whenever possible.
		UINT m_numResourceBarriers = 0;
		std::array<D3D12_RESOURCE_BARRIER, NumResourceBarriersPerBatch> m_resourceBarrierBatch{};
	};

}