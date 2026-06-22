#pragma once

#include "common/memory/arc.h"
#include "universe_core.h"
#include "universe_entity.h"

#include <cstdint>
#include <limits>

namespace Warp
{

using UniverseClusterId = uint64_t;
class UniverseCluster : public ArcMark<UniverseCluster>
{
public:
    static constexpr UniverseClusterId InvalidId = std::numeric_limits<UniverseClusterId>::max();

    UniverseCluster() = default;
    UniverseCluster(const UniverseCluster&) = delete;
    UniverseCluster& operator=(const UniverseCluster&) = delete;

    EUniverseResult Init(UniverseClusterId id) noexcept;
    Arc<UniverseEntityRegistry> GetEntityRegistry() noexcept { return m_entityRegistry; }

    inline constexpr UniverseClusterId GetId() const noexcept { return m_id; }
    inline constexpr bool IsValid() const noexcept { return GetId() != InvalidId; }

private:
    UniverseClusterId m_id = InvalidId;
    Arc<UniverseEntityRegistry> m_entityRegistry;
};

} // namespace Warp
