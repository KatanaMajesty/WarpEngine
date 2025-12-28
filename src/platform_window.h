#pragma once

#include "common/memory/arc.h"
#include "common/enum_flag_set.h"

#include <cstdint>
#include <string_view>
#include <memory>

namespace Warp
{

    struct PlatformWindowInfo
    {
        /// Client width of the window. This will also adjust whole window rectangle on some platforms (Win32)
        uint32_t width = 0;

        /// Client height of the window. This will also adjust whole window rectangle on some platforms (Win32)
        uint32_t height = 0;

        /// The window name.
        /// Currently title will only appear if the window style specifies a title bar (which is constantly true right now in Warp Engine)
        std::string_view title;
    };

    enum class EWindowImpl
    {
        /// Invalid window type, should not be specified when creating a window
        None,

        /// This type specifies that window will be created for Windows platforms.
        Win32,
    };

    enum class EWindowState
    {
        /// On Win32 reflects SW_HIDE. Hides the window and activates another window.
        Hide,
        /// On Win32 reflects SW_SHOWDEFAULT. Sets the show state based on the SW_ value specified in the STARTUPINFO 
        /// structure passed to the CreateProcess function by the program that started the application.
        Show,
    };

    enum class EWindowActionFlag
    {
        /// No actions to be handled this frame
        None,
        /// signals that this window object was resized during this frame and should be handled accordingly
        ResizedThisFrame,
    };

    struct WindowExtent
    {
        uint32_t width;
        uint32_t height;
    };
    
    class IPlatformWindow : public ArcMark<IPlatformWindow>
    {
    public:
        IPlatformWindow() = delete;
        constexpr IPlatformWindow(EWindowImpl impl)
            : m_impl(impl)
        {
        }

        IPlatformWindow(const IPlatformWindow&) = delete;
        IPlatformWindow& operator=(const IPlatformWindow&) = delete;

        virtual ~IPlatformWindow() = default;

        /// Upon initialization window handle is created, but no window is shown on the desktop monitor yet.
        /// @returns true if successfully created platform-specific window handle, otherwise false.
        virtual bool Init(const PlatformWindowInfo& windowInfo) noexcept = 0;

        /// Sets new state for the window. Its behaviour is platform-specific based on the state provided.
        /// For more information on states see EWindowState enumeration.
        virtual bool SetState(EWindowState nextState) noexcept = 0;

        /// Polls window events. After each poll application must check whether window is still active/open.
        /// To check whether the window is still open call 
        virtual void PollEvents() noexcept = 0;

        /// @returns true if window is still alive and open, false otherwise (if it was closed)
        virtual bool IsOpen() const noexcept = 0;

        /// @returns Platform-specific native handle associated with this platform window.
        /// Usually to be used by other modules, like NRI.
        virtual void* GetNativeHandle() const noexcept = 0;

        /// @returns Current window extent that is kept updated by the platform-specific window implementation
        virtual WindowExtent GetCurrentExtent() const noexcept = 0;

        inline constexpr EWindowImpl GetType() const noexcept { return m_impl; }

        inline constexpr FlagSet<EWindowActionFlag> GetActionFlags() const noexcept { return m_windowActionFlags; }

    protected: 
        EWindowImpl m_impl = EWindowImpl::None;
        FlagSet<EWindowActionFlag> m_windowActionFlags = EWindowActionFlag::None;
    };

    Arc<IPlatformWindow> AllocatePlatformWindow(EWindowImpl impl) noexcept;

}