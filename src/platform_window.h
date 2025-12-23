#pragma once

#include "common/memory/arc.h"

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

    enum class EWindowState
    {
        /// On Win32 reflects SW_HIDE. Hides the window and activates another window.
        Hide,
        /// On Win32 reflects SW_SHOWDEFAULT. Sets the show state based on the SW_ value specified in the STARTUPINFO 
        /// structure passed to the CreateProcess function by the program that started the application.
        Show,
    };

    class IPlatformWindow : public ArcMark<IPlatformWindow>
    {
    public:
        IPlatformWindow() = default;

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
    };

    enum class EWindowType
    {
        /// Invalid window type, should not be specified when creating a window
        None,

        /// This type specifies that window will be created for Windows platforms.
        Win32,
    };

    Arc<IPlatformWindow> AllocatePlatformWindow(EWindowType type) noexcept;

}