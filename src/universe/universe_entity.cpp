#include "universe_entity.h"

namespace Warp
{

UniverseEntity UniverseEntityRegistry::MakeEntity() noexcept { return GetNativeRegistry().create(); }

EUniverseResult UniverseEntityRegistry::DestroyEntity(UniverseEntity entity) noexcept
{
    GetNativeRegistry().destroy(entity);
    return EUniverseResult::Ok;
}

EUniverseResult UniverseEntityRegistry::DestroyAllEntities() noexcept
{
    GetNativeRegistry().clear();
    return EUniverseResult::Ok;
}

} // namespace Warp