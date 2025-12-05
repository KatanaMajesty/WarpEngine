#include "platform_window.h"

#include "common/assert.h"

#include "platform_win32_window.h"

namespace Warp
{

    std::shared_ptr<IPlatformWindow> AllocatePlatformWindow(EWindowType type) noexcept
    {
#if defined(_WIN32)
        // Just sanity-check whether EWindowType is properly set on Windows OS
        WARP_ASSERT(type == EWindowType::Win32, "EWindowType::Win32 is required to create window on Windows platforms");

        auto platformWindow = std::make_shared<Win32Window>();
        return platformWindow;

#else // if !defined(_WIN32)
        WARP_ASSERT(type != EWindowType::Win32, "EWindowType::Win32 specified on non-Windows platform");
        WARP_ASSERT(false, "Non-Windows platforms are not yet supported by Platform Window API");
        return nullptr;
#endif // defined(_WIN32)
    }

} // Warp namespace