#include "universe_context.h"

#include "common/assert.h"
#include "common/cross_log.h"

namespace Warp
{

Arc<UniverseCluster> UniverseContext::MakeNewCluster() noexcept
{
    // avoid 0 as it might confuse later, UniverseClusterId::max() is invalid id, 0 still OK
    static UniverseClusterId lastClusterId = UniverseClusterId{1};

    UniverseClusterId nextClusterId = lastClusterId + 1;
    WARP_ASSERT(!m_clusters.contains(nextClusterId), "Anomaly in cluster table. Id {} already exists?", nextClusterId);
    Arc<UniverseCluster> cluster = Arc<UniverseCluster>::Make();
    if (cluster->Init(nextClusterId) != EUniverseResult::Ok)
    {
        WARP_LOG_WARN(LOGGER_UNIVERSE, "Failed to make new Universe cluster with id {}", nextClusterId);
        return nullptr;
    }
    m_clusters[nextClusterId] = cluster;
    ++lastClusterId;
    return cluster;
}

Arc<UniverseCluster> UniverseContext::GetCluster(UniverseClusterId clusterId) noexcept
{
    auto it = m_clusters.find(clusterId);
    return it == m_clusters.end() ? nullptr : it->second;
}

Arc<UniverseCluster> UniverseContext::AbandonCluster(UniverseClusterId clusterId) noexcept
{
    auto it = m_clusters.find(clusterId);
    if (it == m_clusters.end())
    {
        return nullptr;
    }
    Arc<UniverseCluster> cluster = it->second;
    m_clusters.erase(clusterId);
    return cluster;
}

EUniverseResult UniverseContext::SubmitTick() noexcept
{
    for (auto [clusterId, cluster] : m_clusters)
    {
    }
    return EUniverseResult::Ok;
}

} // namespace Warp