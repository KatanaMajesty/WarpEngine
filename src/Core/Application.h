#pragma once

#include <memory>
#include <string>
#include <filesystem>

#include "Input.h"
#include "../Util/Timer.h"
#include "../Assets/Asset.h"
#include "../World/World.h"
#include "../Renderer/Renderer.h"
#include "../Assets/AssetManager.h"
#include "../Assets/MeshImporter.h"
#include "../Assets/TextureImporter.h"
#include "../WinAPI.h"

namespace Warp
{

	using EApplicationFlags = uint32_t;
	enum EApplicationFlag
	{
		EApplicationFlag_None = 0,

		// Allows for multiple resize callbacks when a window is resized
		// If this flag is not specified, resize callback will only be sent after the resizing has finished
		// and unblocked the main thread, after which the Application::Tick() method will be called
		EApplicationFlag_InstantResize = 1,
	};

	struct ApplicationDesc
	{
		std::filesystem::path WorkingDirectory;
		EApplicationFlags Flags = EApplicationFlag_None;
	};

	class Application
	{
	private:
		Application(const ApplicationDesc& desc);

	public:
		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		// Creates an application, allocating memory for it
		static bool Create(const ApplicationDesc& desc);

		// Shuts the application down, deallocates the memory and invalidates all pointers
		static void Delete();
		
		// Getter that returns an application, assuming there is one. 
		// Calling this function when there was no application created using Application::Create() is considered undefined behavior
		static Application& Get();

		// Initialize (or reinitialize) the Application
		void Init(HWND hwnd);
		void RequestResize(uint32_t width, uint32_t height);

		void Tick();
		void Update(float timestep);
		void Render();

		inline InputManager& GetInputManager() { return m_inputManager; }
		inline Renderer* GetRenderer() const { return m_renderer.get(); }

		// Returns the working directory
		inline const std::filesystem::path& GetWorkingDirectory() const { return m_workingDirectory; }
		inline const std::filesystem::path& GetShaderPath() const { return m_shaderPath; }
		inline const std::filesystem::path& GetAssetsPath() const { return m_assetsPath; }
		inline void SetShaderPath(const std::filesystem::path& path) { m_shaderPath = path; }
		inline void SetAssetsPath(const std::filesystem::path& path) { m_assetsPath = path; }

		inline bool IsWindowFocused() const noexcept { return m_isWindowFocused; }
		inline void SetWindowFocused(bool focused) noexcept { m_isWindowFocused = focused; }

		inline World* GetWorld() const { return m_world.get(); }
		inline MeshImporter* GetMeshImporter() const { return m_meshImporter.get(); }
		inline TextureImporter* GetTextureImporter() const { return m_textureImporter.get(); }

	private:
		// Moved to private after Application::RequestResize() was introduced
		void Resize();

		static inline Application* s_instance = nullptr;

		HWND m_hwnd = nullptr;

		Timer m_appTimer;
		double m_lastFrameTime = 0.0;

		InputManager m_inputManager;

		std::unique_ptr<Renderer> m_renderer;
		std::filesystem::path m_workingDirectory;
		std::filesystem::path m_shaderPath;
		std::filesystem::path m_assetsPath;
		bool m_isWindowFocused = true;

		EApplicationFlags m_flags;
		bool m_scheduledResize = false;
		uint32_t m_width = 0;
		uint32_t m_height = 0;

		// TODO: currently we store world in Application. This should be changed though
		std::unique_ptr<World> m_world;
		std::unique_ptr<AssetManager> m_assetManager;
		std::unique_ptr<MeshImporter> m_meshImporter;
		std::unique_ptr<TextureImporter> m_textureImporter;
	};

}
