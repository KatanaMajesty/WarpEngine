#include "platform_win32_window.h"

/// Only include Win32 implementations on Win32 platforms
#if defined(_WIN32)
#include "common/assert.h"
#include "common/cross_log.h"

#include <string>

#define WARP_WIN32_WINDOW_CLASS_NAME LPCSTR("WARP_WND_CLASSNAME")

namespace Warp
{

    Win32Window::~Win32Window()
    {
        // Platform-window handle might not have been initialized.
        // Only clean-up if properly setup previously.
        if (IsInitialized())
        {
            UnregisterClass(WARP_WIN32_WINDOW_CLASS_NAME, m_instanceHandle);
        }
    }

    /// @brief Returns last error recorded by WinAPI as std::string. Last error is obtained using GetLastError() function
    /// see https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror
    static std::string GetLastErrorAsString() noexcept
    {
        // Get the error message ID, if any.
        DWORD errorMessageID = ::GetLastError();
        if (errorMessageID == 0)
        {
            return std::string(); // No error message has been recorded
        }
        // Ask Win32 to give us the string version of that message ID.
        // The parameters we pass in, tell Win32 to create the buffer that holds the message for us
        LPSTR messageBuffer = nullptr;
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     NULL,
                                     errorMessageID,
                                     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                     (LPSTR)&messageBuffer,
                                     0,
                                     NULL);

        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);
        return message;
    }


    struct WindowReference
    {
        Win32Window* window;
    };

    // TODO: Currently nothing sophisticated is needed for WindowProc so it will most likely stay here as it is for a long while
    //      but will probably need to add more support to input handling via callbacks
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        // window handle might be null for some events
        Win32Window* window = reinterpret_cast<Win32Window*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
        switch (uMsg)
        {
        case WM_DESTROY: PostQuitMessage(0); return 0;
        case WM_SIZE:
        {
            WARP_ASSERT(window != nullptr, "Invalid Win32Window pointer");
            UINT width = LOWORD(lParam);
            UINT height = HIWORD(lParam);
            window->SetCurrentExtent(width, height);
            // TODO: add checks whether extent actually changed and only then set action flag
            // set window resize action flag for this frame
            window->GetActionFlagsRef() |= EWindowActionFlag::ResizedThisFrame;
            return 0;
        }
            //    case WM_KILLFOCUS:
            //    case WM_SETFOCUS:
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    bool Win32Window::Init(const PlatformWindowInfo& windowInfo) noexcept
    {
        // First validate if Win32Window is not yet initialized
        WARP_ASSERT(!IsInitialized(), "Window was already initialized before");
        WARP_ASSERT(m_instanceHandle == NULL, "Native handle was previously assigned for this Window");
        m_instanceHandle = GetModuleHandle(nullptr);

        WARP_ASSERT(windowInfo.width > 0 && windowInfo.height > 0, "Window extent is incorrectly set");

        // Register window class
        WNDCLASSEX windowClass;
        ZeroMemory(&windowClass, sizeof(WNDCLASSEX));
        windowClass.cbSize = sizeof(WNDCLASSEX);
        windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        windowClass.lpfnWndProc = WindowProc;
        windowClass.cbClsExtra = 0; // The number of extra bytes to allocate following the window-class structure
        windowClass.cbWndExtra = 0; // The number of extra bytes to allocate following the window instance
        windowClass.hInstance = m_instanceHandle;
        windowClass.hIcon = NULL;   // TODO: Set an Icon
        windowClass.hIconSm = NULL; // TODO: Set an Icon. Win 4.0 only. A handle to a small icon that is associated with the window class.
        windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
        windowClass.lpszClassName = WARP_WIN32_WINDOW_CLASS_NAME;
        // If the function fails, the return value is zero. To get extended error information, call GetLastError.
        // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclassexa
        if (RegisterClassEx(&windowClass) == 0)
        {
            WARP_LOG_ERROR(ELoggerType::DefaultLogger, "RegisterClassEx -> Failed to register window class: {}", GetLastErrorAsString());
            return false;
        }

        RECT windowRect = RECT{ 0, 0, (LONG)windowInfo.width, (LONG)windowInfo.height };
        // If the function fails, the return value is zero. To get extended error information, call GetLastError.
        // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-adjustwindowrect
        if (AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE) == 0)
        {
            WARP_LOG_ERROR(ELoggerType::DefaultLogger, "AdjustWindowRect -> Failed to adjust window to client extent: {}", GetLastErrorAsString());
            return false;
        }

        uint32_t windowWidth = windowRect.right - windowRect.left;
        uint32_t windowHeight = windowRect.bottom - windowRect.top;
        m_nativeHandle = CreateWindow(WARP_WIN32_WINDOW_CLASS_NAME,
                                      windowInfo.title.data(),
                                      WS_OVERLAPPEDWINDOW,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      windowWidth,
                                      windowHeight,
                                      NULL,
                                      NULL,
                                      m_instanceHandle,
                                      NULL);

        // WND struct happens to contain one pointer-sized field, which is not used by the system.
        // It can be accessed using GWLP_USERDATA to store a pointer to user data - in our case 'this' handle
        //
        // If the function fails, the return value is zero.
        // BUT! If the previous value is zero and the function succeeds, the return value is zero, but the function does not clear the last error information.
        // To determine success or failure, clear the last error information by calling SetLastError with 0, then call SetWindowLongPtr
        // Function failure will be indicated by a return value of zero and a GetLastError result that is nonzero.
        SetLastError(0);
        if (SetWindowLongPtrA(m_nativeHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this)) == 0 && GetLastError() != 0)
        {
            WARP_LOG_ERROR(ELoggerType::DefaultLogger, "SetWindowLongPtrA -> Failed to setup GWLP_USERDATA for Win32Window handle: {}", GetLastErrorAsString());
            return false;
        }

        m_extent = WindowExtent{ .width = windowWidth, .height = windowHeight };
        return m_nativeHandle != NULL;
    }

    bool Win32Window::SetState(EWindowState nextState) noexcept
    {
        int32_t cmdShow = -1;
        switch (nextState)
        {
        case EWindowState::Hide: cmdShow = SW_HIDE; break;
        case EWindowState::Show: cmdShow = SW_SHOWDEFAULT; break;
        default: WARP_ASSERT(false, "Unknown EWindowState?"); return false;
        }
        WARP_ASSERT(cmdShow != -1, "Invalid cmdShow");
        ShowWindow(m_nativeHandle, cmdShow);

        // The UpdateWindow function updates the client area of the specified window by sending WM_PAINT message.
        // The WM_PAINT message is sent when the system or another application makes a request to paint a portion of an application's window
        //
        // TODO: Check if update is required after updating state?
        UpdateWindow(m_nativeHandle);
        return true;
    }

    void Win32Window::PollEvents() noexcept
    {
        // before polling new events clear up all previously set window action flags
        m_windowActionFlags = EWindowActionFlag::None;

        while (PeekMessageW(&m_lastMsg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&m_lastMsg);
            DispatchMessage(&m_lastMsg);
        }
    }

    bool Win32Window::IsOpen() const noexcept { return m_lastMsg.message != WM_QUIT; }

} // Warp namespace
#endif // defined(_WIN32)