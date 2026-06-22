#pragma once

#include "common/attributes.h"
#include "common/memory/arc.h"
#include "universe_core.h"

#include <entt/entt.hpp>
#include <type_traits>

namespace Warp
{

/// @brief https://github.com/skypjack/entt/wiki/Entity-Component-System#the-registry-the-entity-and-the-component
using UniverseEntity = entt::entity;
class UniverseEntityRegistry : public ArcMark<UniverseEntityRegistry>
{
public:
    UniverseEntityRegistry() = default;
    UniverseEntityRegistry(const UniverseEntityRegistry&) = delete;
    UniverseEntityRegistry& operator=(const UniverseEntityRegistry&) = delete;

    EUniverseResult Init() noexcept { return EUniverseResult::Ok; }

    UniverseEntity MakeEntity() noexcept;
    EUniverseResult DestroyEntity(UniverseEntity entity) noexcept;
    EUniverseResult DestroyAllEntities() noexcept;

    template <typename Component>
        requires(std::is_aggregate_v<Component>)
    WARP_MAYBE_UNUSED Component* LinkComponent(UniverseEntity entity, Component&& component = {}) noexcept
    {
        // avoid undefined behaviour with emplace-or-replace
        return GetNativeRegistry().emplace_or_replace<Component>(entity, std::forward<Component>(component));
    }

    template <typename... Components>
    WARP_NODISCARD("Do not discard views that are constructed from component look-up")
    auto EntityView() noexcept
    {
        return GetNativeRegistry().view<Components...>();
    }

    inline constexpr entt::registry& GetNativeRegistry() noexcept { return m_nativeRegistry; }

private:
    entt::registry m_nativeRegistry;
};

} // namespace Warp
