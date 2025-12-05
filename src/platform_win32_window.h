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
        Win32Window() = default;

        Win32Window(const Win32Window&) = delete;
        Win32Window& operator=(const Win32Window&) = delete;

        virtual ~Win32Window();

        virtual bool Init(const PlatformWindowInfo& windowInfo) noexcept;

        virtual bool SetState(EWindowState nextState) noexcept;

        virtual void* GetNativeHandle() const noexcept { return m_nativeHandle; }

    private:
        inline constexpr bool IsInitialized() const noexcept { return m_nativeHandle != NULL; }

        HWND m_nativeHandle = NULL;

        /// @brief Instance handle associated with HWND of this Win32Window handle.
        /// This instance is obtained using GetModuleHandle(nullptr)
        HINSTANCE m_instanceHandle = NULL;
    };

} // Warp namespace
#endif // defined(_WIN32)