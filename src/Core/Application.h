#pragma once

#include <memory>
#include <string>
#include <filesystem>
#include <Windows.h>

#include "../Util/Timer.h"

namespace Warp
{

	class Renderer;

	class Application
	{
	private:
		Application(const std::filesystem::path& workingDirectory);

	public:
		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		// Creates an application, allocating memory for it
		static bool Create(const std::filesystem::path& workingDirectory);

		// Shuts the application down, deallocates the memory and invalidates all pointers
		static void Delete();
		
		// Getter that returns an application, assuming there is one. 
		// Calling this function when there was no application created using Application::Create() is considered undefined behavior
		static Application& Get();

		// Initialize (or reinitialize) the Application
		void Init(HWND hwnd);
		void Resize(uint32_t width, uint32_t height);

		void Tick();
		void Update(float timestep);
		void Render();

		inline Renderer* GetRenderer() const { return m_renderer.get(); }

		// Returns the working directory
		inline constexpr const std::filesystem::path& GetWorkingDirectory() const { return m_workingDirectory; }

		// Getters and setters for absolute Shader and Assets paths
		inline constexpr const std::filesystem::path& GetShaderPath() const { return m_shaderPath; }
		inline constexpr const std::filesystem::path& GetAssetsPath() const { return m_assetsPath;  }
		inline constexpr void SetShaderPath(const std::filesystem::path& path) { m_shaderPath = path; }
		inline constexpr void SetAssetsPath(const std::filesystem::path& path) { m_assetsPath = path; }

		inline constexpr bool IsWindowFocused() const noexcept { return m_isWindowFocused; }
		inline void SetWindowFocused(bool focused) noexcept { m_isWindowFocused = focused; }

	private:
		static inline Application* s_instance = nullptr;

		Timer m_appTimer;
		double m_lastFrameTime = 0.0;

		std::unique_ptr<Renderer> m_renderer;
		std::filesystem::path m_workingDirectory;
		std::filesystem::path m_shaderPath;
		std::filesystem::path m_assetsPath;
		bool m_isWindowFocused = true;
	};

}
