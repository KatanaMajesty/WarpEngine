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
		EulersCameraComponent& cameraComponent = m_worldCamera.AddComponent<EulersCameraComponent>(EulersCameraComponent{
			.Pitch = 0.0f,
			.Yaw = -90.0f,
			.EyePos = Math::Vector3(0.0f),
			.UpDir = Math::Vector3(0.0f, 1.0f, 0.0f),
		});
		cameraComponent.SetView();
	}

	void World::Update(float timestep)
	{
		m_timeElapsed += timestep;

		m_entityRegistry.view<TransformComponent>().each(
			[this](TransformComponent& transformComponent)
			{
				float pitch = m_timeElapsed;
				transformComponent.Rotation = Math::Vector3(0.0f, pitch, 0.0f);
			}
		);

		EulersCameraComponent& cameraComponent = m_worldCamera.GetComponent<EulersCameraComponent>();
		bool dirtyView = false;

		InputManager& inputManager = Application::Get().GetInputManager();
		int64_t cx = inputManager.GetCursorX();
		int64_t cy = inputManager.GetCursorY();
		if (inputManager.IsButtonPressed(eMouseButton_Left))
		{
			if (!m_firstLMBClick)
			{
				m_firstLMBClick = true;
				m_lastCursorX = cx;
				m_lastCursorY = cy;
			}

			if (cx != m_lastCursorX || cy != m_lastCursorY)
			{
				constexpr float Sensitivity = 0.4f;

				float xoffset = (cx - m_lastCursorX) * Sensitivity;
				float yoffset = (cy - m_lastCursorY) * Sensitivity;
				m_lastCursorX = cx;
				m_lastCursorY = cy;

				dirtyView = true;
				cameraComponent.Yaw += xoffset;
				cameraComponent.Pitch = Math::Clamp(cameraComponent.Pitch + yoffset, -89.0f, 89.0f);
			}
		}
		else m_firstLMBClick = false;

		float delta = 4.0f * timestep;
		if (inputManager.IsKeyPressed(eKeycode_W))
		{
			dirtyView = true;
			cameraComponent.EyePos += cameraComponent.EyeDir * delta;
		}

		if (inputManager.IsKeyPressed(eKeycode_S))
		{
			dirtyView = true;
			cameraComponent.EyePos -= cameraComponent.EyeDir * delta;
		}

		Math::Vector3 eyeRight = cameraComponent.EyeDir.Cross(cameraComponent.UpDir);
		if (inputManager.IsKeyPressed(eKeycode_A))
		{
			dirtyView = true;
			cameraComponent.EyePos -= eyeRight * delta;
		}

		if (inputManager.IsKeyPressed(eKeycode_D))
		{
			dirtyView = true;
			cameraComponent.EyePos += eyeRight * delta;
		}

		if (dirtyView)
			cameraComponent.SetView();
	}

	void World::Resize(uint32_t width, uint32_t height)
	{
		m_width = width;
		m_height = height;

		EulersCameraComponent& cameraComponent = m_worldCamera.GetComponent<EulersCameraComponent>();
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