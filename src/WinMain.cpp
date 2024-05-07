#include <cstdint>

#include "WinAPI.h"
#include "WinWrap.h"
#include "Core/Application.h"
#include "Core/Logger.h"

static constexpr LPCSTR g_ClassName = "WarpEngineClass";

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Warp::Application& application = Warp::Application::Get();
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            application.RequestResize(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;
    case WM_KILLFOCUS:
        application.SetWindowFocused(false);
        break;
    case WM_SETFOCUS:
        application.SetWindowFocused(true);
        break;
    }

    Warp::WinWrap::IO::ProcessInput(hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// https://learn.microsoft.com/en-us/windows/win32/learnwin32/winmain--the-application-entry-point
int32_t WINAPI WinMain(
    _In_        HINSTANCE hInstance,
    _In_opt_    HINSTANCE hPrevInstance, // has no meaning. It was used in 16-bit Windows, but is now always zero.
    _In_        LPSTR pCmdLine,
    _In_        int32_t nCmdShow)
{
    Warp::WinWrap::ScopedCOMLibrary comLibrary;
    Warp::WinWrap::Inn::InitConsole();
    std::vector<std::string> cmdLineArgs = Warp::WinWrap::Inn::ConvertCmdLineArguments(pCmdLine);

    Warp::WinWrap::IO::InitInputMappings();
    Warp::Logging::Init(Warp::Logging::Severity::WARP_SEVERITY_INFO);

    WNDCLASSEX windowClass;
    ZeroMemory(&windowClass, sizeof(WNDCLASSEX));
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS; // As of 18.02.2024 CS_DBLCLKS was added as we want to support those as well in our input system
    windowClass.lpfnWndProc = WindowProc;
    windowClass.cbClsExtra = 0; // The number of extra bytes to allocate following the window-class structure
    windowClass.cbWndExtra = 0; // The number of extra bytes to allocate following the window instance
    windowClass.hInstance = hInstance;
    windowClass.hIcon = NULL; // TODO: Set an Icon
    windowClass.hIconSm = NULL; // TODO: Set an Icon. Win 4.0 only. A handle to a small icon that is associated with the window class.
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = g_ClassName;
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
    /*int32_t posX = (desktopWidth - windowWidth) / 2;
    int32_t posY = (desktopHeight - windowHeight) / 2;*/

    HWND hwnd = CreateWindow(
        g_ClassName,
        "Warp Engine",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowWidth,
        windowHeight,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hwnd)
    {
        // We failed to create window at this point. Just abort
        return -1;
    }

    Warp::ApplicationDesc appDesc = Warp::ApplicationDesc{
        .WorkingDirectory = Warp::WinWrap::Inn::GetModuleDirectory(),
    };

    if (!Warp::Application::Create(appDesc))
    {
        // We failed to create application
        WARP_LOG_FATAL("Failed to create Application");
        return -1;
    }

    Warp::Application& application = Warp::Application::Get();
    application.Init(hwnd);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = { 0 };
    while (msg.message != WM_QUIT)
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        application.Tick();
    }

    // TODO: Application Shutdown
    Warp::Application::Delete();

    UnregisterClass(g_ClassName, hInstance);
    Warp::WinWrap::Inn::StopConsole();

    return (int32_t)msg.wParam;
}
