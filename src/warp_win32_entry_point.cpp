#if defined(_WIN32)

#include "warp_win32_entry_point.h"
#include "platform_window.h"

#include "common/cross_log.h"
#include "nebulae/vulkan/nri_vk_instance.h"
#include "nebulae/vulkan/nri_vk_device.h"
#include "nebulae/vulkan/nri_vk_surface.h"
#include "nebulae/vulkan/nri_vk_swapchain.h"

namespace Warp
{

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            return 0;
            //    case WM_KILLFOCUS:
            //    case WM_SETFOCUS:
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    EFinishCode Main(HINSTANCE instance, std::span<std::string_view> args)
    {
        WARP_INIT_LOGGER(ELoggerType::DefaultLogger, ELogLevel::Trace);
        WARP_INIT_LOGGER(ELoggerType::NriLogger,     ELogLevel::Trace, "Nri");

        
        std::shared_ptr<IPlatformWindow> window = AllocatePlatformWindow(EWindowType::Win32);
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

        // TODO: Move this to IPlatformWindow
        MSG msg = { 0 };
        while (msg.message != WM_QUIT)
        {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        return EFinishCode::Success;
    }

} // Warp namespace

#endif // defined(_WIN32)