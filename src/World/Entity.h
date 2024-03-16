#pragma once

#include <entt/entt.hpp>

namespace Warp
{

	class EntityCapacitor;

	class Entity
	{
	public:
		Entity() = default;
		Entity(EntityCapacitor* capacitor, entt::entity handle)
			: m_capacitor(capacitor)
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

	private:
        // TODO: This function might look weird but I thought it might look better than *this
        // Maybe remove?
        Entity CopySelf() const { return Entity(m_capacitor, m_handle); }

        friend class EntityCapacitor;

        EntityCapacitor* m_capacitor;
		entt::entity m_handle = entt::null;
	};

}