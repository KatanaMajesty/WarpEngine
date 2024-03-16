#pragma once

#include <entt/entt.hpp>
#include <unordered_map>

#include "Entity.h"
#include "EntityCapacitor.h"
#include "../Core/Defines.h"
#include "../Core/Assert.h"

namespace Warp
{

    // TODO: Currently we just assume that entt::entity is uint32_t persistently on every configuration
    // We should not rely on that though. We should switch to using IntegerIDs + IDComponent for better entity storaging

    class World
    {
    public:
        World(const std::string& name = "Unnamed World");

        // TODO: Currently just playing around here
        void Update(float timestep);
        void Resize(uint32_t width, uint32_t height);

        WARP_ATTR_NODISCARD Entity CreateEntity(std::string_view name = "Unnamed");
        WARP_ATTR_NODISCARD Entity RemoveEntity(Entity entity);

        constexpr       EntityCapacitor& GetEntityCapacitor() { return m_entityCapacitor; }
        constexpr const EntityCapacitor& GetEntityCapacitor() const { return m_entityCapacitor; }

        WARP_ATTR_NODISCARD Entity GetWorldCamera() { return m_worldCamera; }

    private:
        std::string m_worldName;
        uint32_t m_width;
        uint32_t m_height;

        // TODO: Currently we just have 1 registry per World. Maybe we should consider requesting a registry for a World in future?
        EntityCapacitor m_entityCapacitor;
        Entity m_worldCamera;

        // TODO: Remove this from here...
        float m_timeElapsed = 0.0f;
        bool m_firstLMBClick = false;
        int64_t m_lastCursorX = 0;
        int64_t m_lastCursorY = 0;
    };

}