#include "World.h"

#include <fmt/format.h>

#include "Entity.h"
#include "Components.h"
#include "../Core/Application.h"

namespace Warp
{

	World::World(const std::string& name)
		: m_worldName(name)
	{
		m_worldCamera = CreateEntity(fmt::format("{} Camera", name));
		CameraComponent& cameraComponent = m_worldCamera.AddComponent<CameraComponent>(CameraComponent{
				.EyePos = Math::Vector3(0.0f),
				.EyeDir = Math::Vector3(0.0f, 0.0f, -1.0f),
				.UpDir = Math::Vector3(0.0f, 1.0f, 0.0f),
				//.Fov = 45.0f, // They are default
				//.NearPlane = 0.1f,
				//.FarPlane = 1000.0f
			});
		cameraComponent.SetView();
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

		InputManager& inputManager = Application::Get().GetInputManager();
		float delta = 4.0f * timestep;

		CameraComponent& cameraComponent = m_worldCamera.GetComponent<CameraComponent>();
		bool dirtyView = false;
		if (inputManager.IsKeyPressed(eKeycode_W))
		{
			cameraComponent.EyePos.z -= delta;
			dirtyView = true;
		}

		if (inputManager.IsKeyPressed(eKeycode_A))
		{
			cameraComponent.EyePos.x -= delta;
			dirtyView = true;
		}

		if (inputManager.IsKeyPressed(eKeycode_S))
		{
			cameraComponent.EyePos.z += delta;
			dirtyView = true;
		}

		if (inputManager.IsKeyPressed(eKeycode_D))
		{
			cameraComponent.EyePos.x += delta;
			dirtyView = true;
		}

		if (dirtyView)
			cameraComponent.SetView();
	}

	void World::Resize(uint32_t width, uint32_t height)
	{
		m_width = width;
		m_height = height;

		CameraComponent& cameraComponent = m_worldCamera.GetComponent<CameraComponent>();
		cameraComponent.SetProjection(width, height);
	}

	Entity World::CreateEntity(std::string_view name)
	{
		entt::entity handle = m_entityRegistry.create();
		Entity entity = Entity(this, handle);

		m_entityContainer.AddEntity(entity);
		entity.AddComponent<NametagComponent>(name);
		return entity;
	}

	Entity World::DestroyEntity(Entity entity)
	{
		m_entityRegistry.destroy(entity.GetHandle());
		return Entity();
	}

	bool World::EntityContainer::HasEntity(Entity entity) const
	{
		return Container.find(entity.GetID()) != Container.end();
	}

	void World::EntityContainer::AddEntity(Entity entity)
	{
		WARP_ASSERT(!HasEntity(entity));
		Container[entity.GetID()] = entity;
	}

	void World::EntityContainer::RemoveEntity(Entity entity)
	{
		WARP_ASSERT(HasEntity(entity));
		Container.erase(entity.GetID());
	}

	//void World::ClearEntities()
	//{
	//	for (auto& [ID, entity] : Container)
	//	{
	//		WARP_ASSERT(entity.HasComponents<NametagComponent>());
	//		WARP_LOG_INFO("Destroying entity \"{}\"", entity.GetComponent<NametagComponent>().Nametag);

	//		DestroyEntity(entity);
	//	}
	//	m_entityContainer.clear();

	//	// Maybe we don't want to do this as it may be intended for other world's as well in future
	//	m_entityRegistry.clear();
	//}

}