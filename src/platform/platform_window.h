#pragma once

#include "common/memory/arc.h"
#include "common/assert.h"
#include "common/enum_utils.h"
#include "platform/platform_window_events.h"

#include <SDL3/SDL.h>

#include <string_view>
#include <cstdint>
#include <functional>
#include <vector>
#include <array>

namespace Warp
{

struct PlatformWindowCreateInfo
{
    std::string_view title;
    uint32_t width;
    uint32_t height;
};


// If event callback returns true, window will treat dispatched event as "handled" and avoid calling other callbacks in the stack
using PlatformWindowEventCallback = std::function<bool(const PlatformWindowEventBase&)>;
class PlatformWindow : public ArcMark<PlatformWindow>
{
public:
    PlatformWindow() = default;

    PlatformWindow(const PlatformWindow&) = delete;
    PlatformWindow& operator=(const PlatformWindow&) = delete;

    ~PlatformWindow();

    bool Init(const PlatformWindowCreateInfo& createInfo) noexcept;
    void OnWindowEvent(const PlatformWindowEventBase& base) noexcept;
    void RegisterEventCallback(EPlatformWindowEventType eventType, const PlatformWindowEventCallback& callback) noexcept;

    inline void Close() noexcept { DestroySdlWindow(); }
    inline constexpr bool ShouldClose() const noexcept { return m_windowHandle == nullptr; }

    uint32_t GetWidth() const noexcept;
    uint32_t GetHeight() const noexcept;
    inline std::string_view GetTitle() const noexcept { return SDL_GetWindowTitle(m_windowHandle); }
    inline SDL_WindowID GetSdlWindowID() const noexcept { return SDL_GetWindowID(m_windowHandle); }
    inline SDL_Window* GetNativeHandle() const noexcept { return m_windowHandle; }

private:
    void DestroySdlWindow() noexcept;

    SDL_Window* m_windowHandle = nullptr;
    std::array<std::vector<PlatformWindowEventCallback>, EnumValue(EPlatformWindowEventType::NumEventTypes)> m_windowEventCallbacks;
};

} // namespace Warp