#include "Application.h"

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
