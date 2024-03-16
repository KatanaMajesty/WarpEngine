#pragma once

#include <array>
#include <unordered_map>
#include <vector>

#include "stdafx.h"
#include "Resource.h"
#include "../../Core/Defines.h"

namespace Warp
{

    struct RHIResourceStateTracker
    {
        struct PendingResourceBarrier
        {
            RHIResource* Resource;
            UINT SubresourceIndex;
            D3D12_RESOURCE_STATES State;
        };

        // They are the same :)
        using DecayableTransitionBarrier = PendingResourceBarrier;

        RHIResourceStateTracker() = default;

        // Adds a pending resource barrier object to the pending resource barrier queue
        // The pending resource barrier queue will be resolved before executing the command list on the GPU
        // This is needed to resolve and keep track of valid states of unknown resources when they are first used in the command list
        // For more info watch https://www.youtube.com/watch?v=nmB2XMasz2o
        void AddPendingBarrier(const PendingResourceBarrier& barrier);

        // Adds a decayable transition barrier to the list of those.
        // The decayable transition barrier list will be resolved right after execution of the command lists
        // This is needed to correctly store all of the internal state tracking stuff
        // Without calling this method we will either face D3D12 warnings about resource state decays by doing redundant transitions or
        // we will basically store incorrect resource states inside the resource state tracker
        void AddDecayableBarrier(const DecayableTransitionBarrier& barrier);

        void Reset();

        // Queries the cached resource state of a provided resource, if it is not null.
        // If the state of the resource has not been yet cached, it will be added to the cache in UNKNOWN state
        // 
        // The command list is then able to change the cached resource state returned by the function
        CResourceState& GetCachedState(RHIResource* resource);

        auto& GetAllPendingBarriers() { return m_pendingResourceBarriers; }
        auto& GetAllDecayableTransitions() { return m_decayableTransitionBarriers; }

    private:
        std::unordered_map<RHIResource*, CResourceState> m_cachedResourceStates;
        std::vector<PendingResourceBarrier> m_pendingResourceBarriers;
        std::vector<DecayableTransitionBarrier> m_decayableTransitionBarriers;
    };

    // TODO: Fix, RHICommandList should be RHIDeviceChild?

    class RHICommandList
    {
    public:
        RHICommandList() = default;
        RHICommandList(ID3D12Device9* device, D3D12_COMMAND_LIST_TYPE type);

        RHICommandList(const RHICommandList&) = default;
        RHICommandList& operator=(const RHICommandList&) = default;

        RHICommandList(RHICommandList&&) = default;
        RHICommandList& operator=(RHICommandList&&) = default;

        inline constexpr D3D12_COMMAND_LIST_TYPE GetType() const { return m_type; }
        inline ID3D12GraphicsCommandList6* GetD3D12CommandList() const { return m_commandList.Get(); }
        inline ID3D12GraphicsCommandList6* operator->() const { return GetD3D12CommandList(); } // TODO: Will be removed

        void AddTransitionBarrier(RHIResource* resource, D3D12_RESOURCE_STATES state, UINT subresourceIndex = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
        void AddAliasingBarrier(RHIResource* before, RHIResource* after); // NOIMPL
        void AddUavBarrier(RHIResource* resource); // NOIMPL

        void FlushBatchedResourceBarriers();
        void ResolveDecayableResourceStates();
        WARP_ATTR_NODISCARD std::vector<D3D12_RESOURCE_BARRIER> ResolvePendingResourceBarriers();

        void Open(ID3D12CommandAllocator* allocator);
        void Close();

        void SetName(std::wstring_view name);

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

        // TODO: Maybe remove this as it is only used for PIX runtime
        std::string m_name;
    };

}