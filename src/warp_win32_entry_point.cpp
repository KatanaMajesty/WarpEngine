#if defined(_WIN32)

#include "warp_win32_entry_point.h"
#include "platform_window.h"

#include "common/cross_log.h"

#include "nebulae/shader_factory/nri_shader_compiler.h"
#include "nebulae/shader_factory/nri_shader_library.h"

#include "nebulae/vulkan/nri_vk_instance.h"
#include "nebulae/vulkan/nri_vk_device.h"
#include "nebulae/vulkan/nri_vk_surface.h"
#include "nebulae/vulkan/nri_vk_swapchain.h"

#include "nebulae/nri_vk_renderer.h"

// TODO: Remove this after shader tests
#include <string>
#include <fstream>
#include <streambuf>
#include <filesystem>

namespace Warp
{

    EFinishCode Main(HINSTANCE instance, std::span<std::string_view> args)
    {
        WARP_INIT_LOGGER(ELoggerType::DefaultLogger, ELogLevel::Trace);
        WARP_INIT_LOGGER(ELoggerType::NriLogger,     ELogLevel::Trace, "Nri");

        nri::ShaderLibraryConfig shaderLibraryConfig{ .writeSpirvOutputs = true };
        nri::ShaderLibrary::Init(shaderLibraryConfig);

        Arc<IPlatformWindow> window = AllocatePlatformWindow(EWindowImpl::Win32);
        WARP_ASSERT(window != nullptr);
        if (!window->Init(PlatformWindowInfo{ .width = 1280, .height = 720, .title = "Warp application" }))
        {
            WARP_ASSERT(false, "Failed to init window");
            return EFinishCode::Error;
        }
        if (!window->SetState(EWindowState::Show))
        {
            WARP_ASSERT(false, "Failed to show window");
            return EFinishCode::Error;
        }

        auto renderer = Arc<nri::Renderer>::Make();
        renderer->Init(nri::RendererInfo{ .window = window, .numFramesInFlight = 3 });

        while (true)
        {
            window->PollEvents();
            if (!window->IsOpen())
            {
                // if window is closed break before rendering a frame
                break;
            }

            if (window->GetActionFlags() & EWindowActionFlag::ResizedThisFrame)
            {
                // handle resize if window got resized this frame
                renderer->Resize();
            }
            renderer->RenderFrame();
        }

        nri::ShaderLibrary::Deinit();
        return EFinishCode::Success;
    }

} // Warp namespace

#endif // defined(_WIN32)