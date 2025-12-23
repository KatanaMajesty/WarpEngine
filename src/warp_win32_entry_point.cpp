#if defined(_WIN32)

#include "warp_win32_entry_point.h"
#include "platform_window.h"

#include "common/cross_log.h"

#include "nebulae/shader_factory/nri_shader_compiler.h"

#include "nebulae/vulkan/nri_vk_instance.h"
#include "nebulae/vulkan/nri_vk_device.h"
#include "nebulae/vulkan/nri_vk_surface.h"
#include "nebulae/vulkan/nri_vk_swapchain.h"

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

        nri::ShaderCompiler* shaderCompiler = nri::ShaderCompiler::Get();
        WARP_ASSERT(shaderCompiler->Init(), "Failed to initialize shader compiler");

        // TODO: remove this after testing shader compiler
        std::ifstream shaderFile(std::filesystem::path("shaders") / "slang" / "hello_triangle.slang");
        std::string shaderCode(
            (std::istreambuf_iterator<char>(shaderFile)),
            (std::istreambuf_iterator<char>()));

        nri::SlangToSpvValidationInfo slangToSpvValidation;
        nri::SlangToSpvInfo slangToSpvInfo;
        slangToSpvInfo.moduleCode = shaderCode;
        slangToSpvInfo.moduleName = "hello_triangle";
        slangToSpvInfo.entryPointName = "vertexMain";
        slangToSpvInfo.targetProfile = nri::ESpvProfile::Spirv_1_6;
        slangToSpvInfo.validationInfo = &slangToSpvValidation;
        nri::SlangToSpvOutput spirv = shaderCompiler->CompileSlangToSpv(slangToSpvInfo);

        Arc<IPlatformWindow> window = AllocatePlatformWindow(EWindowType::Win32);
        WARP_ASSERT(window != nullptr);

        // create window here and pass its handle to NRI surface and swapchain
        if (!window->Init(PlatformWindowInfo{
                .width = 1280,
                .height = 720,
                .title = "Warp application" }))
        {
            WARP_ASSERT(false, "Failed to init window");
            return EFinishCode::Error;
        }

        if (!window->SetState(EWindowState::Show))
        {
            WARP_ASSERT(false, "Failed to show window");
            return EFinishCode::Error;
        }

        using namespace nri;
        Arc<vk::NriInstance> nriInstance = Arc<vk::NriInstance>::Make(vk::NriInstanceInfo{
            .appName = "Warp test application",
            .appVersion = VK_MAKE_API_VERSION(1, 0, 0, 0),
            .engineName = "Warp Engine",
            .engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0),
            .bSurfaceRequired = true,
        });

        Arc<vk::NriSurface> nriSurface = Arc<vk::NriSurface>::Make(vk::NriSurfaceInfo{
            .instance = nriInstance,
            .type = vk::ESurfaceType::Win32,
            .nativeHandle = window->GetNativeHandle(),
        }); 

        Arc<vk::NriDevice> nriDevice = Arc<vk::NriDevice>::Make(vk::NriDeviceInfo{
            .instance = nriInstance,
            .surface = nriSurface,
            .bSwapchainRequired = true,
        });
        
        Arc<vk::NriSwapchain> nriSwapchain = Arc<vk::NriSwapchain>::Make(vk::NriSwapchainInfo{
            .surface = nriSurface,
            .device = nriDevice,
            .numSwapchainImages = 3,
            // Width/height is now required when creating swapchain
            .width = 1280,
            .height = 720,
        }); 

        while (window->IsOpen())
        {
            window->PollEvents();
        }

        return EFinishCode::Success;
    }

} // Warp namespace

#endif // defined(_WIN32)