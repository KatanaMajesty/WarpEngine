#include "platform_window.h"

#include "common/assert.h"
#include "common/cross_log.h"

#include <span>

namespace Warp
{

PlatformWindow::~PlatformWindow() { DestroySdlWindow(); }

bool PlatformWindow::Init(const PlatformWindowCreateInfo& createInfo) noexcept
{
    WARP_ASSERT(!createInfo.title.empty(), "Window title cannot be empty!");
    WARP_ASSERT(createInfo.width > 0 && createInfo.height > 0, "Invalid window extent specified: {}x{}",
                createInfo.width, createInfo.height);
    SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN;
    m_windowHandle = SDL_CreateWindow(createInfo.title.data(), createInfo.width, createInfo.height, windowFlags);
    if (!m_windowHandle)
    {
        WARP_LOG_ERROR(LOGGER_PLATFORM, "Failed to create SDL window: {}", SDL_GetError());
        return false;
    }
    return true;
}

void PlatformWindow::OnWindowEvent(const PlatformWindowEventBase& base) noexcept
{
    std::span callbacks = m_windowEventCallbacks.at(EnumValue(base.eventType));
    for (const PlatformWindowEventCallback& cb : callbacks)
    {
        // returning true == handled
        if (cb(base))
        {
            break;
        }
    }
}

void PlatformWindow::RegisterEventCallback(EPlatformWindowEventType eventType,
                                           const PlatformWindowEventCallback& callback) noexcept
{
    std::vector<PlatformWindowEventCallback>& callbackStack = m_windowEventCallbacks.at(EnumValue(eventType));
    callbackStack.push_back(callback);
}

uint32_t PlatformWindow::GetWidth() const noexcept
{
    int32_t width;
    SDL_GetWindowSize(m_windowHandle, &width, nullptr);
    return static_cast<uint32_t>(width);
}

uint32_t PlatformWindow::GetHeight() const noexcept
{
    int32_t height;
    SDL_GetWindowSize(m_windowHandle, nullptr, &height);
    return static_cast<uint32_t>(height);
}

void PlatformWindow::DestroySdlWindow() noexcept
{
    if (m_windowHandle)
    {
        SDL_DestroyWindow(m_windowHandle);
        m_windowHandle = nullptr;
    }
}

} // namespace Warp