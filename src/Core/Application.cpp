#include "Application.h"

#include "Logger.h"
#include "Defines.h"
#include "Assert.h"
#include "../Renderer/Renderer.h"

namespace Warp
{

	Application::Application(const std::filesystem::path& workingDirectory)
		// TODO: This is a temporary workaround in order to make our SolutionDir a working directory
		: m_workingDirectory(workingDirectory)
	{
	}

	bool Application::Create(const std::filesystem::path& workingDirectory)
	{
		if (s_instance)
			Delete();

		s_instance = new Application(workingDirectory);
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
	}

	void Application::Resize(uint32_t width, uint32_t height)
	{
		m_renderer->Resize(width, height);
	}

	void Application::Tick()
	{
		Update();
		Render();
	}

	void Application::Update()
	{
	}

	void Application::Render()
	{
		m_renderer->RenderFrame();
	}

}
