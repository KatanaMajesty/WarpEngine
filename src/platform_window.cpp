#include "platform_window.h"

#include "common/assert.h"

#include "platform_win32_window.h"

namespace Warp
{

    Arc<IPlatformWindow> AllocatePlatformWindow(EWindowImpl impl) noexcept
    {
#if defined(_WIN32)
        // Just sanity-check whether EWindowType is properly set on Windows OS
        WARP_ASSERT(impl == EWindowImpl::Win32, "EWindowImpl::Win32 is required to create window on Windows platforms");
        return Arc<Win32Window>::Make();

#else // if !defined(_WIN32)
        WARP_ASSERT(impl != EWindowImpl::Win32, "EWindowImpl::Win32 specified on non-Windows platform");
        WARP_ASSERT(false, "Non-Windows platforms are not yet supported by Platform Window API");
        return nullptr;
#endif // defined(_WIN32)
    }

} // Warp namespace