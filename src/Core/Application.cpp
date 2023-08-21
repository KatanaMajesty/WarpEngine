#include "Application.h"

#include "Logger.h"
#include "Defines.h"
#include "Assert.h"
#include "../Gfx/Renderer.h"

namespace Warp
{

	bool Application::Create()
	{
		if (s_instance)
			Delete();

		s_instance = new Application();
		return true;
	}

	void Application::Delete()
	{
		delete s_instance;
		s_instance = nullptr;
	}

	Application& Application::GetInstance()
	{
		return *s_instance;
	}

	bool Application::Init()
	{
		WARP_MAYBE_UNUSED bool allocRes = AllocateAllComponents();
		WARP_ASSERT(allocRes && "Couldn't allocate all Application components successfully");

		Renderer& renderer = Renderer::GetInstance();
		if (!renderer.Init())
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
	}

	void Application::Update()
	{
	}

	void Application::Render()
	{
	}

}
