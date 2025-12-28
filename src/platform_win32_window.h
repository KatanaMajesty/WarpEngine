#pragma once

/// Only include Win32 implementations on Win32 platforms
#if defined(_WIN32)

#include "platform_window.h"

#define NOMINMAX
#include <Windows.h>

namespace Warp
{

    class Win32Window : public IPlatformWindow
    {
    public:
        constexpr Win32Window()
            : IPlatformWindow(EWindowImpl::Win32)
        {
        }

        Win32Window(const Win32Window&) = delete;
        Win32Window& operator=(const Win32Window&) = delete;

        virtual ~Win32Window();

        virtual bool Init(const PlatformWindowInfo& windowInfo) noexcept;

        virtual bool SetState(EWindowState nextState) noexcept;

        virtual void PollEvents() noexcept;

        virtual bool IsOpen() const noexcept;

        virtual void* GetNativeHandle() const noexcept { return m_nativeHandle; }

        virtual WindowExtent GetCurrentExtent() const noexcept { return m_extent; }

        /// This function should only be accessed by WindowProc
        void SetCurrentExtent(UINT width, UINT height) noexcept { m_extent = { width, height }; }

        FlagSet<EWindowActionFlag>& GetActionFlagsRef() noexcept { return m_windowActionFlags; }

    private:
        inline constexpr bool IsInitialized() const noexcept { return m_nativeHandle != NULL; }

        HWND m_nativeHandle = NULL;

        /// @brief Instance handle associated with HWND of this Win32Window handle.
        /// This instance is obtained using GetModuleHandle(nullptr)
        HINSTANCE m_instanceHandle = NULL;

        /// Last message queried using PeekMessageW
        MSG m_lastMsg = MSG(0);

        /// current extent of the window. This variable is updated each time WM_SIZE event is sent
        WindowExtent m_extent = { 0, 0 };
    };

} // Warp namespace
#endif // defined(_WIN32)