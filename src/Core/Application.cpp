#include "Application.h"

#include "Defines.h"
#include "Assert.h"
#include "../Input/DeviceManager.h"
#include "../Util/Logger.h"

// TODO: Remove
#include "../World/Components.h"

namespace Warp
{

    Application::Application(const ApplicationDesc& desc)
        : m_filepathConfig([&desc]
            {
                // TODO: This might look bad, but it works alright for now. Will be changed in future defo
                const std::filesystem::path relativePath = desc.WorkingDirectory.parent_path().parent_path().parent_path();
                FilepathConfig config = FilepathConfig{
                    .WorkingDirectory = desc.WorkingDirectory,
                    .ShaderDirectory = relativePath / "shaders",
                    .AssetsDirectory = relativePath / "assets",
                };
    return config;
            }())
        // TODO: (14.02.2024) -> Asset manager and importers are on stack. Can cause any problems? Recheck it when youre sane
                , m_assetManager()
                , m_meshImporter(&m_assetManager)
                , m_textureImporter(&m_assetManager)
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

            static void AddEntityFromMesh(
                const std::filesystem::path& assetsPath,
                const std::string& filename,
                AssetManager& manager,
                MeshImporter& meshImporter,
                World* world,
                const TransformComponent& transform)
            {
                std::filesystem::path filepath = assetsPath / filename;
                AssetProxy proxy = meshImporter.ImportStaticMeshFromFile(filepath.string());
                MeshAsset* mesh = manager.GetAs<MeshAsset>(proxy);

                Entity entity = world->CreateEntity(mesh->Name);
                entity.AddComponent<TransformComponent>(transform);
                entity.AddComponent<MeshComponent>(&manager, proxy);
            }

            // TODO: Temp to play with gbuffers
            void Application::OnKeyPressed(const KeyboardDevice::EvKeyInteraction& keyInteraction)
            {
                if (keyInteraction.NextState != eKeycodeState_Pressed)
                {
                    return;
                }

                Application& application = Application::Get();
                RenderOpts& opts = application.m_renderOpts;
                EGbufferType prevType = opts.ViewGbuffer;
                if (keyInteraction.Keycode == eKeycode_Z)
                    opts.ViewGbuffer = prevType == eGbufferType_Albedo ? eGbufferType_NumTypes : eGbufferType_Albedo;

                else if (keyInteraction.Keycode == eKeycode_X)
                    opts.ViewGbuffer = prevType == eGbufferType_Normal ? eGbufferType_NumTypes : eGbufferType_Normal;

                else if (keyInteraction.Keycode == eKeycode_C)
                    opts.ViewGbuffer = prevType == eGbufferType_RoughnessMetalness ? eGbufferType_NumTypes : eGbufferType_RoughnessMetalness;
            }

            void Application::Init(HWND hwnd)
            {
                m_hwnd = hwnd;

                m_renderer = std::make_unique<Renderer>(hwnd);
                m_world = std::make_unique<World>();

                InputDeviceManager& inputManager = InputDeviceManager::Get();
                inputManager.GetKeyboard().AddKeyInteractionDelegate(OnKeyPressed);

                AddEntityFromMesh(GetAssetsPath(), "Sponza/Sponza.gltf",
                    m_assetManager,
                    GetMeshImporter(),
                    GetWorld(),
                    TransformComponent(Math::Vector3(0.0f), Math::Vector3(), Math::Vector3(1.0f))
                );

                /*AddEntityFromMesh(GetAssetsPath(), "antique_camera/AntiqueCamera.gltf",
                    m_assetManager,
                    GetMeshImporter(),
                    GetWorld(),
                    TransformComponent(Math::Vector3(0.0f, -3.0f, -4.0f), Math::Vector3(), Math::Vector3(0.5f))
                );*/

                /*AddEntityFromMesh(GetAssetsPath(), "asteroid/Asteroid.gltf",
                    m_assetManager,
                    GetMeshImporter(),
                    GetWorld(),
                    TransformComponent(Math::Vector3(3.0f, -2.0f, -8.0f), Math::Vector3(), Math::Vector3(1.0f))
                );

                AddEntityFromMesh(GetAssetsPath(), "wood/wood.gltf",
                    m_assetManager,
                    GetMeshImporter(),
                    GetWorld(),
                    TransformComponent(Math::Vector3(0.0f, -3.0f, -4.0f), Math::Vector3(), Math::Vector3(8.0f, 0.05f, 8.0f))
                );*/

                Entity light1 = GetWorld()->CreateEntity("Dirlight1");
                light1.AddComponent<DirectionalLightComponent>(DirectionalLightComponent{
                        .Intensity = 3.2f,
                        .Direction = Math::Vector3(2.0f, -4.0f, -0.5f),
                        .Radiance = Math::Vector3(0.72f, 0.8f, 0.72f)
                    });

                // TODO: Try removing these 2 light sources. Whats the result? Are there uninitialized descriptor warnings?
                Entity light2 = GetWorld()->CreateEntity("Dirlight2");
                light2.AddComponent<DirectionalLightComponent>(DirectionalLightComponent{
                        .Intensity = 2.4f,
                        .Direction = Math::Vector3(-2.0f, -4.0f, 2.0f),
                        .Radiance = Math::Vector3(0.64f, 0.64f, 0.55f)
                    });

                Entity light3 = GetWorld()->CreateEntity("Dirlight3");
                light3.AddComponent<DirectionalLightComponent>(DirectionalLightComponent{
                        .Intensity = 0.8f,
                        .Direction = Math::Vector3(0.75f, -3.66f, 1.0f),
                        .Radiance = Math::Vector3(0.22f, 0.45f, 0.45f)
                    });
            }

            void Application::RequestResize(uint32_t width, uint32_t height)
            {
                m_width = width;
                m_height = height;
                m_scheduledResize = true; // Schedule the resize for the next Tick() call
            }

            void Application::Resize()
            {
                m_renderer->Resize(m_width, m_height);
                m_world->Resize(m_width, m_height);
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
                m_renderer->Render(m_world.get(), m_renderOpts);
            }

}
