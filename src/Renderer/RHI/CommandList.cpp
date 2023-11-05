#include "CommandList.h"

namespace Warp
{

	// =======================
	// RHIResourceStateTracker
	// =======================

	void RHIResourceStateTracker::AddPendingBarrier(const PendingResourceBarrier& barrier)
	{
		m_pendingResourceBarriers.push_back(barrier);
	}

	void RHIResourceStateTracker::Reset()
	{
		m_cachedResourceStates.clear();
		m_pendingResourceBarriers.clear();
	}

	CResourceState& RHIResourceStateTracker::GetCachedState(RHIResource* resource)
	{
		WARP_ASSERT(resource, "Resource cannot be nullptr");
		// If there is none, default-constructed one will be stored, which should be set in a correct state
		CResourceState& state = m_cachedResourceStates[resource];
		if (!state.IsValid())
		{
			state = CResourceState(resource->GetNumSubresources(), D3D12_RESOURCE_STATE_UNKNOWN);
		}
		return state;
	}

	// =======================
	// RHICommandList
	// =======================

	RHICommandList::RHICommandList(ID3D12Device9* device, D3D12_COMMAND_LIST_TYPE type)
		: m_type(type)
		, m_numResourceBarriers(0)
	{
		WARP_ASSERT(device);
		WARP_RHI_VALIDATE(device->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(m_commandList.ReleaseAndGetAddressOf())));
		m_stateTracker.Reset();
	}

	void RHICommandList::AddTransitionBarrier(RHIResource* resource, D3D12_RESOURCE_STATES state, UINT subresourceIndex)
	{
		WARP_ASSERT(resource, "Cannot be nullptr");
		CResourceState& trackedState = m_stateTracker.GetCachedState(resource);

		// If we don't know the resource's state, then we should add it to a pending resource barrier list
		if (trackedState.IsUnknown())
		{
			m_stateTracker.AddPendingBarrier(RHIResourceStateTracker::PendingResourceBarrier{
					.Resource = resource,
					.SubresourceIndex = subresourceIndex,
					.State = state
				});
		}
		else // Otherwise, we do know the resource's state
		{
			UINT numSubresources = trackedState.GetNumSubresources();
			// If we track a resource state on per-subresource level, then we won't be able to get a state of D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
			// thus, we need to manually iterate through all the resource states
			if (subresourceIndex = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && trackedState.IsPerSubresource())
			{
				for (UINT i = 0; i < numSubresources; ++i)
				{
					D3D12_RESOURCE_STATES stateBefore = trackedState.GetSubresourceState(i);
					if (stateBefore == state) // No need to transition, if states are the same
					{
						continue;
					}

					AddResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(
						resource->GetD3D12Resource(),
						stateBefore,
						state,
						i));
				}
			}
			else // Otherwise, we just apply a resource state on per-subresource level
			{
				D3D12_RESOURCE_STATES stateBefore = trackedState.GetSubresourceState(subresourceIndex);
				if (stateBefore != state)
				{
					AddResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(
						resource->GetD3D12Resource(),
						stateBefore,
						state,
						subresourceIndex));
				}
			}
		}

		trackedState.SetSubresourceState(subresourceIndex, state);
	}

	void RHICommandList::AddAliasingBarrier(RHIResource* before, RHIResource* after)
	{
		WARP_YIELD_NOIMPL();
	}

	void RHICommandList::AddUavBarrier(RHIResource* resource)
	{
		WARP_YIELD_NOIMPL();
	}

	void RHICommandList::FlushBatchedResourceBarriers()
	{
		if (m_numResourceBarriers > 0)
		{
			WARP_ASSERT(m_numResourceBarriers < NumResourceBarriersPerBatch, "Internal resource barrier batching error");
			m_commandList->ResourceBarrier(m_numResourceBarriers, m_resourceBarrierBatch.data());
			m_numResourceBarriers = 0;
		}
	}

	WARP_ATTR_NODISCARD std::vector<D3D12_RESOURCE_BARRIER> RHICommandList::ResolvePendingResourceBarriers()
	{
		const auto& pendingBarriers = m_stateTracker.GetAllPendingBarriers();

		std::vector<D3D12_RESOURCE_BARRIER> resolvedBarriers;
		resolvedBarriers.reserve(pendingBarriers.size());

		for (const auto& [resource, subresourceIndex, state] : pendingBarriers)
		{
			WARP_ASSERT(resource && state != D3D12_RESOURCE_STATE_INVALID, "Invalid PendingResourceBarrier entry");

			CResourceState& resourceGlobalState = resource->GetState();
			D3D12_RESOURCE_STATES stateBefore = resourceGlobalState.GetSubresourceState(subresourceIndex);
			D3D12_RESOURCE_STATES stateAfter = (state == D3D12_RESOURCE_STATE_UNKNOWN) ? stateBefore : state;

			// If state before is equal to state after, then no need to transition
			if (stateBefore != stateAfter)
			{
				resolvedBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource->GetD3D12Resource(),
					stateBefore,
					stateAfter,
					subresourceIndex));
			}

			// Get a cached state from previous command list execution
			D3D12_RESOURCE_STATES cachedState = m_stateTracker.GetCachedState(resource).GetSubresourceState(subresourceIndex);
			D3D12_RESOURCE_STATES previousState = (cachedState == D3D12_RESOURCE_STATE_UNKNOWN) ? stateBefore : cachedState;
			WARP_ASSERT(previousState != D3D12_RESOURCE_STATE_INVALID);

			if (previousState != stateBefore)
			{
				resourceGlobalState.SetSubresourceState(subresourceIndex, previousState);
			}
		}

		return resolvedBarriers;
	}

	void RHICommandList::Open(ID3D12CommandAllocator* allocator)
	{
		WARP_RHI_VALIDATE(m_commandList->Reset(allocator, nullptr));
		m_stateTracker.Reset();
		m_numResourceBarriers = 0;
	}

	void RHICommandList::Close()
	{
		FlushBatchedResourceBarriers();
		WARP_RHI_VALIDATE(m_commandList->Close());
	}

	void RHICommandList::SetName(std::wstring_view name)
	{
		WARP_SET_RHI_NAME(m_commandList.Get(), name);
	}

	void RHICommandList::AddResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier)
	{
		if (m_numResourceBarriers + 1 >= NumResourceBarriersPerBatch)
		{
			FlushBatchedResourceBarriers();
		}
		m_resourceBarrierBatch[m_numResourceBarriers] = barrier;
		++m_numResourceBarriers;
	}

}