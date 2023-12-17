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

	void Application::Init(void* hwnd)
	{
		m_renderer = std::make_unique<Renderer>(reinterpret_cast<HWND>(hwnd));
		m_world = std::make_unique<World>();
		m_assetManager = std::make_unique<AssetManager>();
		m_meshImporter = std::make_unique<MeshImporter>(m_renderer.get(), m_assetManager.get());
		m_textureImporter = std::make_unique<TextureImporter>(m_renderer.get(), m_assetManager.get());

		std::filesystem::path filepath = GetAssetsPath() / "antique_camera";

		std::vector<AssetProxy> meshes = m_meshImporter->ImportFromFile((filepath / "AntiqueCamera.gltf").string());
		TransformComponent transform = TransformComponent(Math::Vector3(0.0f, -2.0f, -4.0f), Math::Vector3(), Math::Vector3(0.5f));

		for (size_t i = 0; i < meshes.size(); ++i)
		{
			Entity antiqueEntity = m_world->CreateEntity(fmt::format("Antique Camera Entity Mesh {}", i));
			antiqueEntity.AddComponent<TransformComponent>(transform);
			antiqueEntity.AddComponent<MeshComponent>(m_meshImporter->GetAssetManager(), meshes[i]);
		}
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
		m_world->Update(timestep);
	}

	void Application::Render()
	{
		m_renderer->Render(m_world.get());
	}

}
