#pragma once

#include <vector>
#include <algorithm>

#include "../../Core/Assert.h"
#include "Resource.h"
#include "CommandQueue.h"

namespace Warp
{

    template<typename ResourceType>
    class RHIResourceTrackingContext
    {
    public:
        void Open(RHICommandQueue* queue)
        {
            WARP_ASSERT(m_pendingTrackedResources.empty(), "This tracking context was not closed correctly");
            CleanupCompletedResources(queue);
        }

        void AddTrackedResource(ResourceType&& resource)
        {
            m_pendingTrackedResources.emplace_back(std::forward<ResourceType>(resource));
        }

        void Close(UINT64 fenceValue)
        {
            std::ranges::transform(m_pendingTrackedResources, std::back_inserter(m_resourceTrackedStates),
                [fenceValue](ResourceType& resource) -> TrackedState
                {
                    return TrackedState{
                        .Resource = std::move(resource),
                        .FenceValue = fenceValue
                };
                });
            m_pendingTrackedResources.clear();
        }

        void CleanupCompletedResources(RHICommandQueue* queue)
        {
            std::erase_if(m_resourceTrackedStates, [queue](const TrackedState& trackedState) -> bool
                {
                    return queue->IsFenceComplete(trackedState.FenceValue);
                });
        }

    private:
        struct TrackedState
        {
            ResourceType Resource;
            UINT64 FenceValue;
        };
        std::vector<ResourceType> m_pendingTrackedResources;
        std::vector<TrackedState> m_resourceTrackedStates;
    };

}