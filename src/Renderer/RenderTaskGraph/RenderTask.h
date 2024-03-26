#pragma once

#include <set>
#include <functional>

#include "ResourceCommon.h"
#include "ResourceManager.h"

namespace Warp
{

    class RTGResourceManager;
    class RHICommandContext;

    enum class ERTGDependencyType
    {
        Read,
        Write,
        ReadWrite,
    };

    class RTGRenderTask
    {
    public:
        using DependencyContainer = std::set<RTGResourceProxy>;
        using ExecuteCallback = std::function<void(RHICommandContext*, RTGRenderTask*, RTGResourceManager*)>;

        RTGRenderTask() = default;

        RTGRenderTask(const RTGRenderTask&) = default;
        RTGRenderTask& operator=(const RTGRenderTask&) = default;

        RTGRenderTask(RTGRenderTask&&) = default;
        RTGRenderTask& operator=(RTGRenderTask&&) = default;

        RTGRenderTask&  AddResourceDependency(const RTGResourceProxy& resource, ERTGDependencyType type);
        bool            HasResourceDependency(const RTGResourceProxy& resource, ERTGDependencyType type) const;

        void SetExecuteCallback(const ExecuteCallback& callback) { m_ExecuteCallback = callback; }
        const ExecuteCallback& GetExecuteCallback() const { return m_ExecuteCallback; }

        DependencyContainer& GetReadDependencies() { return m_ReadDependencies; }
        const DependencyContainer& GetReadDependencies() const { return m_ReadDependencies; }

        DependencyContainer& GetWriteDependencies() { return m_WriteDependencies; }
        const DependencyContainer& GetWriteDependencies() const { return m_WriteDependencies; }

        bool IsEmpty() const { return m_ReadDependencies.empty() && m_WriteDependencies.empty(); }
        bool IsValid() const { return m_ExecuteCallback != nullptr; }

    private:
        void AddReadDependency(const RTGResourceProxy& resource) { m_ReadDependencies.insert(resource); }
        bool HasReadDependency(const RTGResourceProxy& resource) const { return m_ReadDependencies.contains(resource); }

        void AddWriteDependency(const RTGResourceProxy& resource) { m_WriteDependencies.insert(resource); }
        bool HasWriteDependency(const RTGResourceProxy& resource) const { return m_WriteDependencies.contains(resource); }

        DependencyContainer m_ReadDependencies;
        DependencyContainer m_WriteDependencies;
        ExecuteCallback m_ExecuteCallback = nullptr;
    };

}