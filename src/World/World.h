#pragma once

#include <entt/entt.hpp>
#include <unordered_map>

#include "Entity.h"
#include "../Core/Defines.h"
#include "../Core/Assert.h"

namespace Warp
{

	// TODO: Currently we just assume that entt::entity is uint32_t persistently on every configuration
	// We should not rely on that though. We should switch to using IntegerIDs + IDComponent for better entity storaging

	class World
	{
	public:
		using EntityRegistry = entt::registry;
		using EntityContainer = std::unordered_map<uint32_t, Entity>;

		World() = default;

		~World();

		// TODO: Currently just playing around here
		void Update(float timestep);

		WARP_ATTR_NODISCARD Entity CreateEntity(std::string_view name = "Unnamed");
		void DestroyEntity(Entity entity);

		inline constexpr auto& GetEntityRegistry() { return m_entityRegistry; }
		inline constexpr auto& GetEntityRegistry() const { return m_entityRegistry; }

		template<typename... ComponentTypes>
		inline auto ViewOf() { return GetEntityRegistry().view<ComponentTypes...>(); }

	private:
		auto FindEntityByID(Entity entity) const -> EntityContainer::const_iterator;
		bool HasEntity(Entity entity) const;
		void AddEntityToContainer(Entity entity);
		void RemoveEntityFromContainer(Entity entity);
		void ClearEntities();

		float m_timeElapsed = 0.0f; // TODO: Remove this from here...
		
		// TODO: Currently we just have 1 registry per World. Maybe we should consider requesting a registry for a World in future?
		EntityRegistry	m_entityRegistry;
		EntityContainer	m_entityContainer;
	};

	template<typename ComponentType, typename... Args>
	auto Entity::AddComponent(Args&&... args) -> ComponentType&
	{
		WARP_ASSERT(IsValid());
		return m_world->GetEntityRegistry().emplace<ComponentType>(m_handle, std::forward<Args>(args)...);
	}

	template<typename ComponentType>
	auto Entity::GetComponent() -> ComponentType&
	{
		WARP_ASSERT(HasComponents<ComponentType>());
		return m_world->GetEntityRegistry().get<ComponentType>(m_handle);
	}

	template<typename ComponentType>
	auto Entity::GetComponent() const -> const ComponentType&
	{
		WARP_ASSERT(HasComponents<ComponentType>());
		return m_world->GetEntityRegistry().get<ComponentType>(m_handle);
	}

	template<typename ComponentType>
	bool Entity::RemoveComponent()
	{
		WARP_ASSERT(IsValid());
		size_t numRemoved = m_world->GetEntityRegistry().remove<ComponentType>(m_handle);
		return numRemoved > 0;
	}

	template<typename... ComponentTypes>
	bool Entity::HasComponents() const
	{
		WARP_ASSERT(IsValid());
		return m_world->GetEntityRegistry().all_of<ComponentTypes...>(m_handle);
	}

	inline bool Entity::IsValid() const
	{
		return m_world->GetEntityRegistry().valid(m_handle);
	}

}