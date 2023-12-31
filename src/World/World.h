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
		World(const std::string& name = "Unnamed World");

		// TODO: Currently just playing around here
		void Update(float timestep);
		void Resize(uint32_t width, uint32_t height);

		WARP_ATTR_NODISCARD Entity CreateEntity(std::string_view name = "Unnamed");
		WARP_ATTR_NODISCARD Entity DestroyEntity(Entity entity);

		inline constexpr auto& GetEntityRegistry() { return m_entityRegistry; }
		inline constexpr auto& GetEntityRegistry() const { return m_entityRegistry; }
		inline Entity GetWorldCamera() { return m_worldCamera; }

	private:
		// TODO: Do we even need this EntityContainer? Maybe remove?
		struct EntityContainer
		{
			bool HasEntity(Entity entity) const;
			void AddEntity(Entity entity);
			void RemoveEntity(Entity entity);

			std::unordered_map<uint32_t, Entity> Container;
		};
		
		std::string m_worldName;
		uint32_t m_width;
		uint32_t m_height;

		// TODO: Currently we just have 1 registry per World. Maybe we should consider requesting a registry for a World in future?
		entt::registry m_entityRegistry;
		EntityContainer	m_entityContainer;
		Entity m_worldCamera;

		// TODO: Remove this from here...
		float m_timeElapsed = 0.0f;
		bool m_firstLMBClick = false;
		int64_t m_lastCursorX = 0;
		int64_t m_lastCursorY = 0;
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