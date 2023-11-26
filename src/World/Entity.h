#pragma once

#include <entt/entt.hpp>

namespace Warp
{

	class World;

	class Entity
	{
	public:
		Entity() = default;
		Entity(World* world, entt::entity handle)
			: m_world(world)
			, m_handle(handle)
		{
		}

		template<typename ComponentType, typename... Args>
		auto AddComponent(Args&&... args) -> ComponentType&;

		template<typename ComponentType>
		auto GetComponent() -> ComponentType&;

		template<typename ComponentType>
		auto GetComponent() const -> const ComponentType&;

		template<typename ComponentType>
		bool RemoveComponent();
		
		template<typename... ComponentTypes>
		bool HasComponents() const;

		inline bool IsValid() const;

		auto GetHandle() const { return m_handle; }
		auto GetID() const { return static_cast<uint32_t>(GetHandle()); } // Currently we use entt::entity as its ID

	private:
		World* m_world;
		entt::entity m_handle = entt::null;
	};

}