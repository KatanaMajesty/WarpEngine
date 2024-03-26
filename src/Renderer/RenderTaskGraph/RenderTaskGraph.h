#pragma once

#include <set>
#include <functional>
#include <memory>

#include "RenderTask.h"
#include "ResourceCommon.h"
#include "TaskAllocator.h"

#include "../RHI/CommandContext.h"

namespace Warp
{

    class RenderTaskGraph
    {
    public:
        RenderTaskGraph() = default;

        RenderTaskGraph(const RenderTaskGraph&) = default;
        RenderTaskGraph& operator=(const RenderTaskGraph&) = default;

        RenderTaskGraph(RenderTaskGraph&&) = default;
        RenderTaskGraph& operator=(RenderTaskGraph&&) = default;

        // Task that is retrieved as a result of AddRenderTask() call should be immediately initialized in-place
        RTGRenderTask* AddRenderTask();
        void Execute(RHICommandContext* context, RTGResourceManager* resourceManager);
        void Compile();
        void Cleanup();

    private:
        using AdjacencyListEntryType = std::vector<size_t>;
        using AdjacencyListType = std::vector<AdjacencyListEntryType>;

        void PushGraphAdjacencyDependencies(size_t taskIdx, const RTGResourceProxy& resource, AdjacencyListEntryType& entry, ERTGDependencyType type) const;

        struct TaskTraversalGuard
        {
            bool IsVisited = false;
            bool IsOnStack = false;
        };

        // Orders tasks topologically, placing them in AdjacencyList container
        void OrderTasks(AdjacencyListType& adjacencyList, std::vector<size_t>& outputIndices) const;
        void OrderTasksDescend(size_t taskIdx, AdjacencyListType& adjacencyList, std::vector<TaskTraversalGuard>& traversalGuards, std::vector<size_t>& outputIndices) const;

        // Contains those render tasks that can be executed in parallel (that is the longest path from root)
        // The lower the level the more dependency distance in DAG corresponding tasks have, meaning they should be executed after those with lower Level
        struct TaskDependencyGroup
        {
            uint32_t Level = 0;
            std::vector<size_t> RenderTaskIndices;
        };

        // Initializes task indices into different dependency group based on their distance from root in DAG
        void InitDependencyLevels(const AdjacencyListType& adjacencyList, const std::vector<size_t>& topologicalTaskIndices);

        RTGTaskAllocator<RTGRenderTask> m_RenderTaskAllocator;
        std::vector<RTGRenderTask*>     m_RenderTasks;

        std::vector<TaskDependencyGroup> m_DependencyLevels;
    };

}