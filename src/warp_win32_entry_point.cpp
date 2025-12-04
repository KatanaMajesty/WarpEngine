#if defined(_WIN32)

#include "warp_win32_entry_point.h"

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

        // create window here and pass its handle to NRI instance creation info
        LPCSTR winClassName = "Warp window class";
        LPCSTR winName = "Warp Application Window";

        WNDCLASSEX windowClass;
        ZeroMemory(&windowClass, sizeof(WNDCLASSEX));
        windowClass.cbSize = sizeof(WNDCLASSEX);
        windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        windowClass.lpfnWndProc = WindowProc;
        windowClass.cbClsExtra = 0; // The number of extra bytes to allocate following the window-class structure
        windowClass.cbWndExtra = 0; // The number of extra bytes to allocate following the window instance
        windowClass.hInstance = instance;
        windowClass.hIcon = NULL;   // TODO: Set an Icon
        windowClass.hIconSm = NULL; // TODO: Set an Icon. Win 4.0 only. A handle to a small icon that is associated with the window class.
        windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
        windowClass.lpszClassName = winClassName;
        RegisterClassEx(&windowClass);

        RECT desktopRect;
        GetClientRect(GetDesktopWindow(), &desktopRect);

        uint32_t desktopWidth = desktopRect.right - desktopRect.left;
        uint32_t desktopHeight = desktopRect.bottom - desktopRect.top;
        uint32_t clientWidth = 1280;
        uint32_t clientHeight = 720;

        RECT windowRect = RECT{ 0, 0, (LONG)clientWidth, (LONG)clientHeight };
        AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

        uint32_t windowWidth = windowRect.right - windowRect.left;
        uint32_t windowHeight = windowRect.bottom - windowRect.top;

        HWND hwnd = CreateWindow(
            winClassName,
            winName,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            windowWidth,
            windowHeight,
            NULL,
            NULL,
            instance,
            NULL);

        if (!hwnd)
        {
            // We failed to create window at this point. Just abort
            return EFinishCode::Error;
        }

        ShowWindow(hwnd, SW_SHOWDEFAULT /*nCmdShow*/);
        UpdateWindow(hwnd);

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
            .nativeHandle = hwnd,
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
        }); 

        MSG msg = { 0 };
        while (msg.message != WM_QUIT)
        {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        UnregisterClass(winClassName, instance);
        return EFinishCode::Success;
    }

} // Warp namespace

#endif // defined(_WIN32)