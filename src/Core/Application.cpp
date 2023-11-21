#include "Application.h"

#include "Logger.h"
#include "Defines.h"
#include "Assert.h"
#include "../Renderer/Renderer.h"
#include "../World/World.h"

#include "../Assets/ModelLoader.h"

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

	void Application::Init(HWND hwnd)
	{
		m_renderer = std::make_unique<Renderer>(hwnd);
		m_world = std::make_unique<World>();
		m_assetManager = std::make_unique<AssetManager>();

		ModelLoader loader(m_assetManager.get());
		m_tempModelProxy = loader.Load((GetAssetsPath() / "cube" / "cube.gltf").string());
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
		m_renderer->Update(timestep);
	}

	void Application::Render()
	{
		// TODO: Temporarily using Model asset here. Will be removed asap
		ModelAsset* model = m_assetManager->GetAs<ModelAsset>(m_tempModelProxy);
		m_renderer->RenderFrame(model);
	}

}
