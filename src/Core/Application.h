#pragma once

#include <memory>
#include <string>
#include <filesystem>

#include "../Util/Timer.h"
#include "../Input/KeyboardDevice.h"
#include "../World/World.h"

#include "../Assets/Asset.h"
#include "../Assets/AssetManager.h"
#include "../Assets/Importers/MeshImporter.h"
#include "../Assets/Importers/TextureImporter.h"

#include "../Renderer/Renderer.h"
#include "../WinAPI.h"

namespace Warp
{

	struct ApplicationDesc
	{
		std::filesystem::path WorkingDirectory;
	};

	class Application
	{
	private:
		Application(const ApplicationDesc& desc);

	public:
		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		Application(Application&&) = delete;
		Application operator=(Application&&) = delete;

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

		inline Renderer* GetRenderer() const { return m_renderer.get(); }

		// Returns the working directory
		inline const std::filesystem::path& GetWorkingDirectory() const { return m_filepathConfig.WorkingDirectory; }
		inline const std::filesystem::path& GetShaderPath() const { return m_filepathConfig.ShaderDirectory; }
		inline const std::filesystem::path& GetAssetsPath() const { return m_filepathConfig.AssetsDirectory; }

		inline bool IsWindowFocused() const noexcept { return m_isWindowFocused; }
		inline void SetWindowFocused(bool focused) noexcept { m_isWindowFocused = focused; }

		inline World* GetWorld() const { return m_world.get(); }
		inline MeshImporter& GetMeshImporter() { return m_meshImporter; }
		inline TextureImporter& GetTextureImporter() { return m_textureImporter; }

	private:
		// Moved to private, use Application::RequestResize() instead
		void Resize();

		static inline Application* s_instance = nullptr;

		HWND m_hwnd = nullptr;

		Timer m_appTimer;
		double m_lastFrameTime = 0.0;

		struct FilepathConfig
		{
			std::filesystem::path WorkingDirectory;
			std::filesystem::path ShaderDirectory;
			std::filesystem::path AssetsDirectory;
		};
		const FilepathConfig m_filepathConfig; // For now it is const, we do not plan on ever changing it (yet)

		std::unique_ptr<Renderer> m_renderer;
		bool m_isWindowFocused = true;
		bool m_scheduledResize = false;
		uint32_t m_width = 0;
		uint32_t m_height = 0;

		// TODO: currently we store world in Application. This should be changed though
		std::unique_ptr<World> m_world;
		AssetManager m_assetManager;
		MeshImporter m_meshImporter;
		TextureImporter m_textureImporter;
		
		// TODO: Temp, remove when played with gbuffers enough
		static void OnKeyPressed(const KeyboardDevice::EvKeyInteraction& keyInteraction);
		RenderOpts m_renderOpts;
	};

}
