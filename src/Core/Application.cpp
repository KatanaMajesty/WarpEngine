#include "Application.h"

#include "Logger.h"
#include "Defines.h"
#include "Assert.h"

#include "../Assets/MeshImporter.h"
#include "../Assets/TextureImporter.h"

// TODO: Remove
#include "../World/Components.h"

namespace Warp
{

	Application::Application(const ApplicationDesc& desc)
		// TODO: This is a temporary workaround in order to make our SolutionDir a working directory
		: m_workingDirectory(desc.WorkingDirectory)
		, m_flags(desc.Flags)
	{
	}

	bool Application::Create(const ApplicationDesc& desc)
	{
		if (s_instance)
			Delete();

		s_instance = new Application(desc);
		return true;
	}

	void Application::Delete()
	{
		delete s_instance;
		s_instance = nullptr;
	}

	Application& Application::Get()
	{
		return *s_instance;
	}

	static void AddEntityFromMesh(
		const std::filesystem::path& assetsPath, 
		const std::string& filename, 
		MeshImporter* meshImporter, 
		World* world, 
		const TransformComponent& transform)
	{
		std::filesystem::path filepath = assetsPath / filename;
		std::vector<AssetProxy> meshes = meshImporter->ImportFromFile(filepath.string());

		for (size_t i = 0; i < meshes.size(); ++i)
		{
			Entity antiqueEntity = world->CreateEntity(fmt::format("{} Mesh {}", filename, i));
			antiqueEntity.AddComponent<TransformComponent>(transform);
			antiqueEntity.AddComponent<MeshComponent>(meshImporter->GetAssetManager(), meshes[i]);
		}
	}

	void Application::Init(HWND hwnd)
	{
		m_hwnd = hwnd;

		m_renderer = std::make_unique<Renderer>(hwnd);
		m_world = std::make_unique<World>();
		m_assetManager = std::make_unique<AssetManager>();
		m_meshImporter = std::make_unique<MeshImporter>(m_renderer.get(), m_assetManager.get());
		m_textureImporter = std::make_unique<TextureImporter>(m_renderer.get(), m_assetManager.get());

		AddEntityFromMesh(GetAssetsPath(), "antique_camera/AntiqueCamera.gltf", 
			GetMeshImporter(), 
			GetWorld(),
			TransformComponent(Math::Vector3(0.0f, -3.0f, -4.0f), Math::Vector3(), Math::Vector3(0.5f))
		);

		AddEntityFromMesh(GetAssetsPath(), "asteroid/Asteroid.gltf",
			GetMeshImporter(),
			GetWorld(),
			TransformComponent(Math::Vector3(3.0f, -2.0f, -8.0f), Math::Vector3(), Math::Vector3(1.0f))
		);

		AddEntityFromMesh(GetAssetsPath(), "wood/wood.gltf",
			GetMeshImporter(),
			GetWorld(),
			TransformComponent(Math::Vector3(0.0f, -3.0f, -4.0f), Math::Vector3(), Math::Vector3(8.0f, 0.05f, 8.0f))
		);

		Entity light1 = GetWorld()->CreateEntity("Dirlight1");
		light1.AddComponent<DirectionalLightComponent>(DirectionalLightComponent{
				.Intensity = 0.4f,
				.Direction = Math::Vector3(-1.0f, -2.0f, -1.0f),
				.Radiance = Math::Vector3(0.76f, 0.89f, 0.98f)
			});

		Entity light2 = GetWorld()->CreateEntity("Dirlight2");
		light2.AddComponent<DirectionalLightComponent>(DirectionalLightComponent{
				.Intensity = 0.4f,
				.Direction = Math::Vector3(-1.0f, -1.0f, 1.0f),
				.Radiance = Math::Vector3(0.98f, 0.89f, 0.78f)
			});

		Entity light3 = GetWorld()->CreateEntity("Dirlight3");
		light3.AddComponent<DirectionalLightComponent>(DirectionalLightComponent{
				.Intensity = 0.4f,
				.Direction = Math::Vector3(0.75f, -0.66f, 1.0f),
				.Radiance = Math::Vector3(0.22f, 0.45f, 0.45f)
			});
	}

	void Application::RequestResize(uint32_t width, uint32_t height)
	{
		m_width = width;
		m_height = height;
		if (m_flags & EApplicationFlag_InstantResize)
		{
			// Do all the resizing
			Resize();
		}
		else m_scheduledResize = true; // Else schedule the resize for the next Tick() call
	}

	void Application::Resize()
	{
		m_renderer->Resize(m_width, m_height);
		m_world->Resize(m_width, m_height);
	}

	void Application::Tick()
	{
		if (m_scheduledResize)
		{
			m_scheduledResize = false;
			Resize();
		}

		double elapsed = m_appTimer.GetElapsedSeconds();
		double timestep = elapsed - m_lastFrameTime;
		m_lastFrameTime = elapsed;

		Update((float)timestep);
		Render();
	}

	void Application::Update(float timestep)
	{
		// TODO: Maybe remove this to Main.cpp? I thought it might be nice to move all the WinAPI stuff there
		// and just to some callbacks to Application
		POINT point;
		if (GetCursorPos(&point) && ScreenToClient(m_hwnd, &point))
		{
			int64_t x = point.x;
			int64_t y = int64_t(m_height) - point.y;
			m_inputManager.SetCursorPos(x, y);
		}

		m_world->Update(timestep);
	}

	void Application::Render()
	{
		m_renderer->Render(m_world.get());
	}

}
