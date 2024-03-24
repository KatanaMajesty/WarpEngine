#include "RenderTaskGraph.h"

#include <functional>

namespace Warp
{

    RTGRenderTask* RenderTaskGraph::AddRenderTask()
    {
        RTGRenderTask* task = m_RenderTaskAllocator.AllocateTask();
        m_RenderTasks.push_back(task);
        return task;
    }

    void RenderTaskGraph::Execute()
    {
        Compile();

        for (TaskDependencyGroup& dependencyGroup : m_DependencyLevels)
        {
            for (size_t taskIdx : dependencyGroup.RenderTaskIndices)
            {
                RTGRenderTask* renderTask = m_RenderTasks[taskIdx];
                assert(renderTask->IsValid() && !renderTask->IsEmpty());

                std::invoke(renderTask->GetExecuteCallback(), renderTask, m_ResourceManager);
            }
        }

        Cleanup();
    }

    void RenderTaskGraph::Compile()
    {
        // For compilation-execution we chill here: https://levelup.gitconnected.com/organizing-gpu-work-with-directed-acyclic-graphs-f3fd5f2c2af3
        const size_t numRenderTasks = m_RenderTasks.size();

        // Build an adjacency list
        AdjacencyListType adjacencyList;
        adjacencyList.reserve(numRenderTasks);

        for (size_t taskIdx = 0; taskIdx < numRenderTasks; ++taskIdx)
        {
            RTGRenderTask* task = m_RenderTasks[taskIdx];

            AdjacencyListEntryType& entry = adjacencyList.emplace_back();
            for (const RTGResourceProxy& dependency : task->GetReadDependencies())
            {
                PushGraphAdjacencyDependencies(taskIdx, dependency, entry, ERTGDependencyType::Write);
            }
        }

        std::vector<size_t> taskIndices;
        OrderTasks(adjacencyList, taskIndices);
        InitDependencyLevels(adjacencyList, taskIndices);
    }

    void RenderTaskGraph::Cleanup()
    {
        m_DependencyLevels.clear();
        m_RenderTasks.clear();
        m_RenderTaskAllocator.Cleanup();
    }

    void RenderTaskGraph::PushGraphAdjacencyDependencies(size_t taskIdx, const RTGResourceProxy& resource, AdjacencyListEntryType& entry, ERTGDependencyType type) const
    {
        for (size_t i = 0; i < m_RenderTasks.size(); ++i)
        {
            if (taskIdx == i)
            {
                continue;
            }

            RTGRenderTask* task = m_RenderTasks[i];
            if (task->HasResourceDependency(resource, type))
            {
                entry.push_back(i);
            }
        }
    }

    void RenderTaskGraph::OrderTasks(AdjacencyListType& adjacencyList, std::vector<size_t>& outputIndices) const
    {
        const size_t numTasks = adjacencyList.size();
        assert(numTasks > 0);

        outputIndices.clear();
        outputIndices.reserve(numTasks);

        std::vector<TaskTraversalGuard> traversalGuards(numTasks);

        for (size_t taskIdx = 0; taskIdx < numTasks; ++taskIdx)
        {
            OrderTasksDescend(taskIdx, adjacencyList, traversalGuards, outputIndices);
        }

        std::ranges::reverse(outputIndices);
    }

    void RenderTaskGraph::OrderTasksDescend(size_t taskIdx,
        AdjacencyListType& adjacencyList,
        std::vector<TaskTraversalGuard>& traversalGuards,
        std::vector<size_t>& outputIndices) const
    {
        // We just assume that we are not out of bounds for provided taskIdx - otherwise its just UB
        TaskTraversalGuard& currentGuard = traversalGuards[taskIdx];

        if (currentGuard.IsVisited)
        {
            if (currentGuard.IsOnStack)
            {
                // We have met circular dependency - abort!
                throw std::runtime_error("RenderTaskGraph::OrderTasksDescend -> Circular dependency found. Aborting the application");
            }

            return;
        }

        AdjacencyListEntryType& entry = adjacencyList[taskIdx];
        const size_t numEntries = entry.size();
        currentGuard.IsOnStack = true;
        currentGuard.IsVisited = true;

        for (size_t entryIdx = 0; entryIdx < numEntries; ++entryIdx)
        {
            size_t currentTaskIdx = entry[entryIdx];
            OrderTasksDescend(currentTaskIdx, adjacencyList, traversalGuards, outputIndices);
        }

        currentGuard.IsOnStack = false;

        // Finally, push the task's index
        outputIndices.push_back(taskIdx);
    }

    void RenderTaskGraph::InitDependencyLevels(const AdjacencyListType& adjacencyList, const std::vector<size_t>& topologicalTaskIndices)
    {
        const size_t numTasks = topologicalTaskIndices.size();

        std::vector<uint32_t> taskDistances(numTasks, std::numeric_limits<uint32_t>::min());
        for (size_t taskIdx : topologicalTaskIndices)
        {
            for (size_t dependentTaskIdx : adjacencyList[taskIdx])
            {
                taskDistances[dependentTaskIdx] = std::max(taskDistances[dependentTaskIdx], taskDistances[taskIdx] + 1);
            }
        }

        uint32_t maxDistance = *std::ranges::max_element(taskDistances);
        m_DependencyLevels.clear();
        m_DependencyLevels.resize(maxDistance + 1);

        for (size_t taskIdx = 0; taskIdx < numTasks; ++taskIdx)
        {
            // we need to inverse the distance because we use write-to-read dependencies (Not sure why is that a problem though)
            uint32_t level = maxDistance - taskDistances[taskIdx];

            TaskDependencyGroup& dependencyGroup = m_DependencyLevels[level];
            dependencyGroup.Level = level;
            dependencyGroup.RenderTaskIndices.push_back(taskIdx);
        }
    }

}