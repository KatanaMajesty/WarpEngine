#include "Application.h"

#include "Logger.h"
#include "Defines.h"
#include "Assert.h"

// TODO: Remove
#include "../World/Components.h"

namespace Warp
{

	Application::Application(const ApplicationDesc& desc)
		// TODO: This is a temporary workaround in order to make our SolutionDir a working directory
		: m_workingDirectory(desc.WorkingDirectory)
		, m_flags(desc.Flags)
		// TODO: (14.02.2024) -> Asset manager and importers are on stack. Can cause any problems? Recheck it when youre sane
		, m_assetManager()
		, m_meshImporter(&m_assetManager)
		, m_textureImporter(&m_assetManager)
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
		AssetManager& manager,
		MeshImporter& meshImporter, 
		World* world, 
		const TransformComponent& transform)
	{
		std::filesystem::path filepath = assetsPath / filename;
		AssetProxy proxy = meshImporter.ImportStaticMeshFromFile(filepath.string());
		MeshAsset* mesh = manager.GetAs<MeshAsset>(proxy);

		Entity entity = world->CreateEntity(mesh->Name);
		entity.AddComponent<TransformComponent>(transform);
		entity.AddComponent<MeshComponent>(&manager, proxy);
	}

	void Application::Init(HWND hwnd)
	{
		m_hwnd = hwnd;

		m_renderer = std::make_unique<Renderer>(hwnd);
		m_world = std::make_unique<World>();

		AddEntityFromMesh(GetAssetsPath(), "antique_camera/AntiqueCamera.gltf", 
			m_assetManager,
			GetMeshImporter(), 
			GetWorld(),
			TransformComponent(Math::Vector3(0.0f, -3.0f, -4.0f), Math::Vector3(), Math::Vector3(0.5f))
		);

		AddEntityFromMesh(GetAssetsPath(), "wetmud/Wetmud.gltf",
			m_assetManager,
			GetMeshImporter(),
			GetWorld(),
			TransformComponent(Math::Vector3(3.0f, -2.0f, -8.0f), Math::Vector3(), Math::Vector3(1.0f))
		);

		AddEntityFromMesh(GetAssetsPath(), "wood/wood.gltf",
			m_assetManager,
			GetMeshImporter(),
			GetWorld(),
			TransformComponent(Math::Vector3(0.0f, -3.0f, -4.0f), Math::Vector3(), Math::Vector3(8.0f, 0.05f, 8.0f))
		);

		Entity light1 = GetWorld()->CreateEntity("Dirlight1");
		light1.AddComponent<DirectionalLightComponent>(DirectionalLightComponent{
				.Intensity = 2.4f,
				.Direction = Math::Vector3(-1.0f, -2.0f, -1.0f),
				.Radiance = Math::Vector3(0.76f, 0.89f, 0.98f)
			});

		// TODO: Try removing these 2 light sources. Whats the result? Are there uninitialized descriptor warnings?
		Entity light2 = GetWorld()->CreateEntity("Dirlight2");
		light2.AddComponent<DirectionalLightComponent>(DirectionalLightComponent{
				.Intensity = 1.4f,
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
		InputManager& inputManager = m_inputManager;

		RenderOpts opts = RenderOpts();
		if (inputManager.IsKeyPressed(eKeycode_Z))
		{
			opts.ViewGbuffer = eGbufferType_Albedo;
		}
		else if (inputManager.IsKeyPressed(eKeycode_X))
		{
			opts.ViewGbuffer = eGbufferType_Normal;
		}
		else if (inputManager.IsKeyPressed(eKeycode_C))
		{
			opts.ViewGbuffer = eGbufferType_RoughnessMetalness;
		}

		m_renderer->Render(m_world.get(), opts);
	}

}
