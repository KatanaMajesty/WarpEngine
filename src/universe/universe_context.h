#pragma once

#include "common/attributes.h"

#include "universe_cluster.h"
#include "universe_core.h"

#include <unordered_map>

namespace Warp
{

/// @brief Universe represents a context of everything in the Warp
class UniverseContext
{
public:
    UniverseContext() = default;
    UniverseContext(const UniverseContext&) = delete;
    UniverseContext& operator=(const UniverseContext&) = delete;

    /// @brief Creates new Universe cluster (scene piece)
    /// @return a new cluster or nullptr if failed to create new cluster
    Arc<UniverseCluster> MakeNewCluster() noexcept;

    /// @brief Obtains a pointer to a Universe cluster by its id, if valid is provided
    /// @return reference to the cluster or nullptr if no cluster with such id
    Arc<UniverseCluster> GetCluster(UniverseClusterId clusterId) noexcept;

    /// @brief Removes Universe cluster from internal array of alive clusters
    /// @return still alive reference to the removed cluster or nullptr if no cluster with such id
    WARP_MAYBE_UNUSED Arc<UniverseCluster> AbandonCluster(UniverseClusterId clusterId) noexcept;

    /// @brief Performs one logical universe tick (all entity logic)
    /// @return EUniverseResult::Ok if should keep ticking
    EUniverseResult SubmitTick() noexcept;

private:
    std::unordered_map<UniverseClusterId, Arc<UniverseCluster>> m_clusters;
};

}; // namespace Warp
