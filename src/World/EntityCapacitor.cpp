#include "EntityCapacitor.h"

namespace Warp
{

    Entity EntityCapacitor::CreateEntity()
    {
        entt::entity handle = m_registry.create();
        return Entity(this, handle);
    }

    Entity EntityCapacitor::RemoveEntity(Entity entity)
    {
        m_registry.destroy(entity.m_handle);
        return Entity();
    }

    bool EntityCapacitor::IsValid(Entity entity) const
    {
        return m_registry.valid(entity.m_handle);
    }

}