#include "platform_arbiter.h"

#include "common/cross_log.h"
#include "common/assert.h"

namespace Warp
{

bool PlatformArbiter::Init(const PlatformArbiterCreateInfo& createInfo)
{
    // Optionally, specify basic metadata about Warp engine application.
    // https://wiki.libsdl.org/SDL3/SDL_SetAppMetadata
    if (!createInfo.appName.empty() || !createInfo.appVersion.empty() || !createInfo.appId.empty())
    {
        if (!SDL_SetAppMetadata(createInfo.appName.empty() ? "Warp Application" : createInfo.appName.data(),
                                createInfo.appVersion.empty() ? "?" : createInfo.appVersion.data(),
                                createInfo.appId.empty() ? "warpengine.app" : createInfo.appId.data()))
        {
            WARP_LOG_WARN(LOGGER_PLATFORM, "Failed to set SDL3 app metadata: {}", SDL_GetError());
        }
    }
    // SDL initialization. For more info see ref https://wiki.libsdl.org/SDL3/SDL_Init
    SDL_InitFlags subsystemFlags = 0;
    subsystemFlags |= createInfo.subsystems.IsSet(PlatformSubsystem::Audio) ? SDL_INIT_AUDIO : 0;
    subsystemFlags |= createInfo.subsystems.IsSet(PlatformSubsystem::Video) ? SDL_INIT_VIDEO : 0;
    subsystemFlags |= createInfo.subsystems.IsSet(PlatformSubsystem::Joystick) ? SDL_INIT_JOYSTICK : 0;
    subsystemFlags |= createInfo.subsystems.IsSet(PlatformSubsystem::Events) ? SDL_INIT_EVENTS : 0;
    if (!SDL_Init(subsystemFlags))
    {
        WARP_LOG_ERROR(LOGGER_PLATFORM, "Failed to initialize SDL3 subsystem: {}", SDL_GetError());
        return false;
    }
    return true;
}

    void PlatformArbiter::Deinit() noexcept
    {
        CloseAllWindows();
        SDL_Quit();
    }

void PlatformArbiter::PollEvents() noexcept
{
    SDL_Event sdlEvent;
    while (SDL_PollEvent(&sdlEvent))
    {
        switch (sdlEvent.type)
        {
            case SDL_EVENT_TERMINATING: WARP_FALLTHROUGH;
            case SDL_EVENT_QUIT:
                WARP_LOG_TRACE(LOGGER_PLATFORM, "PlatformArbiter::PollEvents -> Closing all windows (SDL_EVENT_QUIT | SDL_EVENT_TERMINATING)");
                this->CloseAllWindows();
                break;
            case SDL_EVENT_WINDOW_SHOWN:
            case SDL_EVENT_WINDOW_HIDDEN:
                break;
            case SDL_EVENT_WINDOW_MOVED:
            {
                PlatformWindowMovedEvent event(sdlEvent.window);
                Arc<PlatformWindow> window = FindWindowBySdlID(sdlEvent.window.windowID);
                if (window)
                {
                    window->OnWindowEvent(event);
                }
                break;
            }
            case SDL_EVENT_WINDOW_RESIZED:
            {
                PlatformWindowResizedEvent event(sdlEvent.window);
                Arc<PlatformWindow> window = FindWindowBySdlID(sdlEvent.window.windowID);
                if (window)
                {
                    window->OnWindowEvent(event);
                }
                break;
            }
            case SDL_EVENT_WINDOW_MINIMIZED:
            case SDL_EVENT_WINDOW_MAXIMIZED:
            // Keyboard focus gained
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
            // Keyboard focus lost
            case SDL_EVENT_WINDOW_FOCUS_LOST:
            case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            {
                PlatformWindowCloseRequestEvent event;
                Arc<PlatformWindow> window = FindWindowBySdlID(sdlEvent.window.windowID);
                if (window)
                {
                    this->RemoveWindowFromTable(window->GetSdlWindowID());
                    window->OnWindowEvent(event);
                    window->Close(); // signal for any thread that is looping while(!ShouldClose)
                }
                break;
            }
            // Keyboard key events
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            // Mouse events
            case SDL_EVENT_MOUSE_MOTION:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_WHEEL:
            default: break;
        }
    }
}

Arc<PlatformWindow> PlatformArbiter::MakeWindow(const PlatformWindowCreateInfo& createInfo) noexcept
{
    Arc<PlatformWindow> window = Arc<PlatformWindow>::Make();
    if (!window->Init(createInfo))
    {
        WARP_LOG_ERROR(LOGGER_PLATFORM, "Failed to create platform window");
        return nullptr;
    }
    AddWindowToTable(window);
    return window;
}

void PlatformArbiter::CloseAllWindows() noexcept
{
    PlatformWindowCloseRequestEvent event;
    for (auto [id, window] : m_platformWindowTable)
    {
        this->RemoveWindowFromTable(window->GetSdlWindowID());
        window->OnWindowEvent(event);
        window->Close();
    }
}

void PlatformArbiter::AddWindowToTable(const Arc<PlatformWindow>& window) noexcept
{
    WARP_ASSERT(FindWindowBySdlID(window->GetSdlWindowID()) == nullptr,
                "Duplicate SDL window ID. PlatformArbiter keeps dead handles in window table");
    m_platformWindowTable[window->GetSdlWindowID()] = window;
}

void PlatformArbiter::RemoveWindowFromTable(SDL_WindowID id) noexcept
{
    WARP_ASSERT(m_platformWindowTable.contains(id), "Platform window with SDL id {} was not found", id);
    m_platformWindowTable.erase(id);
}

Arc<PlatformWindow> PlatformArbiter::FindWindowBySdlID(SDL_WindowID id) const noexcept
{
    auto it = m_platformWindowTable.find(id);
    return it == m_platformWindowTable.end() ? nullptr : it->second;
}

} // namespace Warp