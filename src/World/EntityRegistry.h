#pragma once

#include <entt/entt.hpp>
#include <unordered_map>
#include "Entity.h"

#include "../Core/Defines.h"

namespace Warp
{

	// TODO: Currently we just assume that entt::entity is uint32_t persistently on every configuration
	// We should not rely on that though. We should switch to using IntegerIDs + IDComponent for better entity storaging

	class EntityRegistry
	{
	public:
		template<typename... ComponentTypes>
		inline auto ViewOf() { return GetEntityRegistry().view<ComponentTypes...>(); }

		WARP_ATTR_NODISCARD Entity CreateEntity(std::string_view name = "Unnamed");
		WARP_ATTR_NODISCARD Entity DestroyEntity(Entity entity);

		void Reset();

	private:
		bool HasEntity(Entity entity) const;
		void AddEntity(Entity entity);
		void RemoveEntity(Entity entity);

		entt::registry m_registry;
		std::unordered_map<uint32_t, Entity> m_container;
	};

}