#pragma once

#include <string>

namespace Warp
{

	class Application
	{
	private:
		Application() = default;

	public:
		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		// Creates an application, allocating memory for it
		static bool Create();

		// Shuts the application down, deallocates the memory and invalidates all pointers
		static void Delete();
		
		// Getter that returns an application, assuming there is one. 
		// Calling this function when there was no application created using Application::Create() is considered undefined behavior
		static Application& GetInstance();

		void Tick();
		void Update();
		void Render();

		inline constexpr bool IsWindowFocused() const noexcept { return m_isWindowFocused; }
		inline void SetWindowFocused(bool focused) noexcept { m_isWindowFocused = focused; }

	private:
		static inline Application* s_instance = nullptr;

		bool m_isWindowFocused = true;
	};

}
