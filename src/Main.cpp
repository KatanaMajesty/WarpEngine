#include <Windows.h>

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <string_view>

#include "Core/Application.h"
#include "Core/Logger.h"

WCHAR g_ClassName[] = L"WarpEngineClass";

void InitWin32Console();
void DeinitWin32Console();
void ParseWin32CmdlineParams(std::vector<std::string>& cmdLineArgs, PWSTR pCmdLine);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// https://learn.microsoft.com/en-us/windows/win32/learnwin32/winmain--the-application-entry-point
auto WINAPI wWinMain(
    _In_ HINSTANCE hInstance, 
    _In_opt_ HINSTANCE hPrevInstance, // has no meaning. It was used in 16-bit Windows, but is now always zero.
    _In_ PWSTR pCmdLine, 
    _In_ int32_t nCmdShow) -> int32_t
{
    InitWin32Console();

    Warp::Logging::Init(Warp::Logging::Severity::WARP_SEVERITY_INFO);
    WARP_LOG_INFO("Hello, World {}", 42);

    std::vector<std::string> cmdLineArgs;
    ParseWin32CmdlineParams(cmdLineArgs, pCmdLine);

    WNDCLASSEX windowClass;
    ZeroMemory(&windowClass, sizeof(WNDCLASSEX));
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
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
    int32_t posX = (desktopWidth - windowWidth) / 2;
    int32_t posY = (desktopHeight - windowHeight) / 2;

    HWND hwnd = CreateWindow(
        g_ClassName,
        L"Warp Engine",
        WS_OVERLAPPEDWINDOW,
        posX,
        posY,
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

    if (!Warp::Application::Create())
    {
        // We failed to create application
        WARP_LOG_FATAL("Failed to create Application");
        return -1;
    }

    Warp::Application& application = Warp::Application::GetInstance();
    if (!application.Init())
    {
        WARP_LOG_FATAL("Failed to init Application");
        return -1;
    }

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
        
        // TODO: Application Ticking
        application.Tick();
    }

    // TODO: Application Shutdown
    Warp::Application::Delete();

    UnregisterClassW(g_ClassName, hInstance);
    DeinitWin32Console();

    return (int32_t)msg.wParam;
}

void InitWin32Console()
{
    AllocConsole();
    FILE* dummy; // to avoid deprecation errors
    freopen_s(&dummy, "CONOUT$", "w+", stdout); // stdout will print to the newly created console
}

void DeinitWin32Console()
{
    if (!FreeConsole())
    {
        // Yield warning that no console was freed
    }
}

#include "Util/String.h"
#include <iterator>

void ParseWin32CmdlineParams(std::vector<std::string>& cmdLineArgs, PWSTR pCmdLine)
{
    std::string args;
    Warp::WStringToString(args, pCmdLine);

    std::istringstream iss(args);

    using IteratorType = std::istream_iterator<std::string>;
    cmdLineArgs = std::vector(IteratorType(iss), IteratorType());
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Warp::Application& application = Warp::Application::GetInstance();
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KILLFOCUS:
        application.SetWindowFocused(false);
        break;
    case WM_SETFOCUS:
        application.SetWindowFocused(true);
        break;
    return 0;

    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}