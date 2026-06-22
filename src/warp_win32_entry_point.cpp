#if defined(_WIN32)

#include "warp_win32_entry_point.h"

#include "common/cross_log.h"
#include "common/attributes.h"
#include "nebulae/nri_vk_renderer.h"
#include "nebulae/shader_factory/nri_shader_compiler.h"
#include "nebulae/shader_factory/nri_shader_library.h"
#include "nebulae/vulkan/nri_vk_device.h"
#include "nebulae/vulkan/nri_vk_instance.h"
#include "nebulae/vulkan/nri_vk_swapchain.h"

// TODO: Remove this after shader tests
#include <filesystem>
#include <fstream>
#include <streambuf>
#include <string>

// TODO: remove
#include "platform/platform_arbiter.h"
#include "platform/platform_window.h"

namespace Warp
{

EFinishCode Main(HINSTANCE instance, std::span<std::string_view> args)
{
    WARP_INIT_LOGGER(LOGGER_DEFAULT, ELogLevel::Trace);
    WARP_INIT_LOGGER(LOGGER_PLATFORM, ELogLevel::Trace, "Platform");
    WARP_INIT_LOGGER(LOGGER_NRI, ELogLevel::Trace, "Nri");

    nri::ShaderLibraryConfig shaderLibraryConfig{.writeSpirvOutputs = true};
    nri::ShaderLibrary::Init(shaderLibraryConfig);

    PlatformArbiterCreateInfo platformCreateInfo = {
        .appName = "Warp Engine", .appVersion = "0.1.0", .subsystems = PlatformSubsystem::Video | PlatformSubsystem::Events};
    if (!PlatformArbiter::GetInstance()->Init(platformCreateInfo))
        WARP_UNLIKELY
        {
            WARP_ASSERT(false, "Failed to initialize SDL3 manager");
            return EFinishCode::Error;
        }

    PlatformWindowCreateInfo windowCreateInfo = {.title = "Warp Engine", .width = 1280, .height = 720};
    Arc<PlatformWindow> window = PlatformArbiter::GetInstance()->MakeWindow(windowCreateInfo);
    if (!window)
        WARP_UNLIKELY
        {
            WARP_ASSERT(false, "Failed to initialize SDL3 window");
            return EFinishCode::Error;
        }
    window->RegisterEventCallback(EPlatformWindowEventType::WindowCloseRequest,
                                  [](const PlatformWindowEventBase& base)
                                  {
                                      WARP_LOG_INFO(LOGGER_PLATFORM, "Window is being closed");
                                      return false;
                                  });

    auto renderer = Arc<nri::Renderer>::Make();
    renderer->Init(nri::RendererInfo{.window = window->GetNativeHandle(), .numFramesInFlight = 3});

    window->RegisterEventCallback(
        EPlatformWindowEventType::WindowResized,
        [renderer](const PlatformWindowEventBase& base)
        {
            auto event = reinterpret_cast<const PlatformWindowResizedEvent&>(base);
            WARP_LOG_INFO(LOGGER_PLATFORM, "Window is being resized: {}x{}", event.nextWidth, event.nextHeight);
            renderer->Resize();
            return false;
        });

    while (true)
    {
        PlatformArbiter::GetInstance()->PollEvents();
        if (window->ShouldClose())
        {
            break;
        }
        renderer->RenderFrame();
    }

    PlatformArbiter::GetInstance()->Deinit();
    nri::ShaderLibrary::Deinit();
    return EFinishCode::Success;
}

} // namespace Warp

#endif // defined(_WIN32)