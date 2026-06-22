#pragma once

#include "common/memory/arc.h"
#include "common/attributes.h"
#include "common/enum_flag_set.h"
#include "platform_window.h"

#include <SDL3/SDL.h>

#include <string_view>
#include <unordered_map>

namespace Warp
{

enum class PlatformSubsystem
{
    Audio,
    Video,
    Joystick,
    Events,
};

struct PlatformArbiterCreateInfo
{
    WARP_OPTIONAL(std::string_view) appName;
    WARP_OPTIONAL(std::string_view) appVersion;
    WARP_OPTIONAL(std::string_view) appId;
    FlagSet<PlatformSubsystem> subsystems;
};

class PlatformArbiter
{
private:
    PlatformArbiter() = default;

public:
    PlatformArbiter(const PlatformArbiter&) = delete;
    PlatformArbiter& operator=(const PlatformArbiter&) = delete;

    static PlatformArbiter* GetInstance() noexcept
    {
        static PlatformArbiter instance;
        return &instance;
    }

    /// @brief Initializes platform arbiter with SDL3 backend
    bool Init(const PlatformArbiterCreateInfo& createInfo);
    /// @brief Deinitializes platform arbiter object and all related backends.
    void Deinit() noexcept;
    void PollEvents() noexcept;

    Arc<PlatformWindow> MakeWindow(const PlatformWindowCreateInfo& createInfo) noexcept;

private:
    void CloseAllWindows() noexcept;
    void AddWindowToTable(const Arc<PlatformWindow>& window) noexcept;
    void RemoveWindowFromTable(SDL_WindowID id) noexcept;
    Arc<PlatformWindow> FindWindowBySdlID(SDL_WindowID id) const noexcept;

    std::unordered_map<SDL_WindowID, Arc<PlatformWindow>> m_platformWindowTable;
};

} // namespace Warp