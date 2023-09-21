#include "Application.h"

#include "Logger.h"
#include "Defines.h"
#include "Assert.h"
#include "../Renderer/Renderer.h"

namespace Warp
{

	Application::Application(const std::filesystem::path& workingDirectory)
		// TODO: This is a temporary workaround in order to make our SolutionDir a working directory
		: m_workingDirectory(workingDirectory.parent_path().parent_path())
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

	bool Application::Init(HWND hwnd, uint32_t width, uint32_t height)
	{
		WARP_MAYBE_UNUSED bool allocRes = AllocateAllComponents();
		WARP_ASSERT(allocRes && "Couldn't allocate all Application components successfully");

		Renderer& renderer = Renderer::Get();
		if (!renderer.Init(hwnd, width, height))
		{
			WARP_LOG_FATAL("Failed to initialize Renderer");
			return false;
		}

		return true;
	}

	bool Application::AllocateAllComponents()
	{
		if (!Renderer::Create())
			return false;
	
		return true;
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
		Renderer::Get().Render();
	}

}
