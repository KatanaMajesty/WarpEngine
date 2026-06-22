#pragma once

#include "common/assert.h"
#include "common/enum_utils.h"

#include <SDL3/SDL.h>
#include <cstdint>
#include <string_view>

namespace Warp
{

enum class EPlatformWindowEventType
{
    WindowShown,
    WindowHidden,
    WindowMoved,
    WindowResized,
    WindowMinimized,
    WindowMaximized,
    WindowKeyboardFocusGained,
    WindowKeyboardFocusLost,
    WindowEnterFullscreen,
    WindowLeaveFullscreen,
    WindowCloseRequest,
    NumEventTypes,
};
template <>
constexpr std::string_view EnumString<EPlatformWindowEventType>(EPlatformWindowEventType eventType)
{
    switch (eventType)
    {
        case EPlatformWindowEventType::WindowShown: return "EPlatformWindowEventType::WindowShown";
        case EPlatformWindowEventType::WindowHidden: return "EPlatformWindowEventType::WindowHidden";
        case EPlatformWindowEventType::WindowMoved: return "EPlatformWindowEventType::WindowMoved";
        case EPlatformWindowEventType::WindowResized: return "EPlatformWindowEventType::WindowResized";
        case EPlatformWindowEventType::WindowMinimized: return "EPlatformWindowEventType::WindowMinimized";
        case EPlatformWindowEventType::WindowMaximized: return "EPlatformWindowEventType::WindowMaximized";
        case EPlatformWindowEventType::WindowKeyboardFocusGained:
            return "EPlatformWindowEventType::WindowKeyboardFocusGained";
        case EPlatformWindowEventType::WindowKeyboardFocusLost:
            return "EPlatformWindowEventType::WindowKeyboardFocusLost";
        case EPlatformWindowEventType::WindowEnterFullscreen: return "EPlatformWindowEventType::WindowEnterFullscreen";
        case EPlatformWindowEventType::WindowLeaveFullscreen: return "EPlatformWindowEventType::WindowLeaveFullscreen";
        case EPlatformWindowEventType::WindowCloseRequest: return "EPlatformWindowEventType::WindowCloseRequest";
        default: return "InvalidEnumValue";
    }
}
struct PlatformWindowEventBase
{
    PlatformWindowEventBase(EPlatformWindowEventType eventType)
        : eventType(eventType)
    {
    }
    EPlatformWindowEventType eventType;
};
// Window has been resized to nextWidth and nextHeight
struct PlatformWindowResizedEvent : PlatformWindowEventBase
{
    PlatformWindowResizedEvent(const SDL_WindowEvent& sdlEvent)
        : PlatformWindowEventBase(EPlatformWindowEventType::WindowResized)
        , nextWidth(static_cast<uint32_t>(sdlEvent.data1))
        , nextHeight(static_cast<uint32_t>(sdlEvent.data2))
    {
        WARP_ASSERT(sdlEvent.data1 > 0 && sdlEvent.data2 > 0, "Invalid extents coming from SDL?");
    }
    uint32_t nextWidth;
    uint32_t nextHeight;
};
// Window has been moved to nextPosX, nextPosY
struct PlatformWindowMovedEvent : PlatformWindowEventBase
{
    PlatformWindowMovedEvent(const SDL_WindowEvent& sdlEvent)
        : PlatformWindowEventBase(EPlatformWindowEventType::WindowMoved)
        , nextPosX(sdlEvent.data1)
        , nextPosY(sdlEvent.data2)
    {
    }
    int32_t nextPosX;
    int32_t nextPosY;
};
// The window manager requests that the window be closed
struct PlatformWindowCloseRequestEvent : PlatformWindowEventBase
{
    PlatformWindowCloseRequestEvent()
        : PlatformWindowEventBase(EPlatformWindowEventType::WindowCloseRequest)
    {
    }
};

} // namespace Warp