#include "universe_cluster.h"

namespace Warp
{

EUniverseResult UniverseCluster::Init(UniverseClusterId id) noexcept 
{ 
    m_entityRegistry = Arc<UniverseEntityRegistry>::Make();
    WARP_ASSERT(m_entityRegistry->Init() == EUniverseResult::Ok, "Failed to initialize Universe entity registry");
    return EUniverseResult::Ok; 
}

} // namespace Warp