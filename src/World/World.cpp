#include "World.h"

#include "Entity.h"
#include "Components.h"

namespace Warp
{

	World::~World()
	{
		ClearEntities();
	}

	void World::Update(float timestep)
	{
		m_timeElapsed += timestep;

		ViewOf<TransformComponent>().each(
			[this](TransformComponent& transformComponent)
			{
				float pitch = m_timeElapsed;
				transformComponent.Rotation = Math::Vector3(0.0f, pitch, 0.0f);
			}
		);
	}

	Entity World::CreateEntity(std::string_view name)
	{
		entt::entity handle = m_entityRegistry.create();
		Entity entity = Entity(this, handle);

		AddEntityToContainer(entity);
		entity.AddComponent<NametagComponent>(name);
		return entity;
	}

	void World::DestroyEntity(Entity entity)
	{
		m_entityRegistry.destroy(entity.GetHandle());
	}

	auto World::FindEntityByID(Entity entity) const -> EntityContainer::const_iterator
	{
		return m_entityContainer.find(entity.GetID());
	}

	bool World::HasEntity(Entity entity) const
	{
		return FindEntityByID(entity) != m_entityContainer.end();
	}

	void World::AddEntityToContainer(Entity entity)
	{
		WARP_ASSERT(!HasEntity(entity));
		m_entityContainer[entity.GetID()] = entity;
	}

	void World::RemoveEntityFromContainer(Entity entity)
	{
		WARP_ASSERT(HasEntity(entity));
		m_entityContainer.erase(entity.GetID());
	}

	void World::ClearEntities()
	{
		for (auto& [ID, entity] : m_entityContainer)
		{
			WARP_ASSERT(entity.HasComponents<NametagComponent>());
			WARP_LOG_INFO("Destroying entity \"{}\"", entity.GetComponent<NametagComponent>().Nametag);

			DestroyEntity(entity);
		}
		m_entityContainer.clear();

		// Maybe we don't want to do this as it may be intended for other world's as well in future
		m_entityRegistry.clear();
	}

}