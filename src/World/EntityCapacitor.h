#pragma once

#include <entt/entt.hpp>

#include "Entity.h"
#include "../Core/Defines.h"
#include "../Core/Assert.h"

namespace Warp
{

    class EntityCapacitor
    {
    public:
        template<typename... ComponentTypes>
        using ExcludeWrapperType = entt::exclude_t<ComponentTypes...>;

        EntityCapacitor() = default;

        Entity CreateEntity();
        Entity RemoveEntity(Entity entity);

        template<typename ComponentType, typename... Args>
        auto AddComponent(Entity entity, Args&&... args) -> ComponentType&
        {
            WARP_ASSERT(IsValid(entity));
            return m_registry.emplace<ComponentType>(entity.m_handle, std::forward<Args>(args)...);
        }

        template<typename... ComponentTypes>
        bool HasComponents(Entity entity) const
        {
            WARP_ASSERT(IsValid(entity));
            return m_registry.all_of<ComponentTypes...>(entity.m_handle);
        }

        template<typename ComponentType>
        auto GetComponent(Entity entity) -> ComponentType&
        {
            WARP_ASSERT(HasComponents<ComponentType>(entity));
            return m_registry.get<ComponentType>(entity.m_handle);
        }

        template<typename ComponentType>
        auto GetComponent(Entity entity) const -> const ComponentType&
        {
            WARP_ASSERT(HasComponents<ComponentType>(entity));
            return m_registry.get<ComponentType>(entity.m_handle);
        }

        template<typename ComponentType>
        bool RemoveComponent(Entity entity)
        {
            WARP_ASSERT(IsValid(entity));
            size_t numRemoved = m_registry.remove<ComponentType>(entity.m_handle);
            return numRemoved > 0;
        }

        bool IsValid(Entity entity) const;

        template<typename ComponentType, typename... OtherTypes, typename... ExcludedTypes>
        auto ViewOf(ExcludeWrapperType<ExcludedTypes...> excludes = ExcludeWrapperType())
        {
            return m_registry.view<ComponentType, OtherTypes...>(excludes);
        }

    private:
        friend class Entity;

        entt::registry& GetEntityRegistry() { return m_registry; }
        entt::registry m_registry;
    };

    template<typename ComponentType, typename... Args>
    auto Entity::AddComponent(Args&&... args) -> ComponentType&
    {
        return m_capacitor->AddComponent<ComponentType>(CopySelf(), std::forward<Args>(args)...);
    }

    template<typename ComponentType>
    auto Entity::GetComponent() -> ComponentType&
    {
        return m_capacitor->GetComponent<ComponentType>(CopySelf());
    }

    template<typename ComponentType>
    auto Entity::GetComponent() const -> const ComponentType&
    {
        return m_capacitor->GetComponent<ComponentType>(CopySelf());
    }

    template<typename ComponentType>
    bool Entity::RemoveComponent()
    {
        return m_capacitor->RemoveComponent<ComponentType>(CopySelf());
    }

    template<typename... ComponentTypes>
    bool Entity::HasComponents() const
    {
        return m_capacitor->HasComponents<ComponentTypes...>(CopySelf());
    }

    inline bool Entity::IsValid() const
    {
        return m_capacitor->IsValid(CopySelf());
    }

}