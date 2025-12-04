#include "nri_vk_surface.h"

namespace Warp::nri::vk
{
    
    NriSurface::NriSurface(const NriSurfaceInfo& surfaceInfo)
        : m_instance(surfaceInfo.instance)
    {
        // For now we just assume that surface is needed, because Warp Engine won't currently support headless rendering
        WARP_ASSERT(surfaceInfo.type != ESurfaceType::None,
            "Vulkan NRI surface requires a type for presentation, but ESurfaceType::None was specified!");
        WARP_ASSERT(surfaceInfo.nativeHandle != nullptr,
            "Vulkan NRI surface requires a valid native window handle for presentation, but nullptr was specified!");

        // Create platform-specific surface for presentation
#ifdef VK_USE_PLATFORM_WIN32_KHR
        if (surfaceInfo.type == ESurfaceType::Win32)
        {
            auto surfaceCreateInfo = NRI_VK_STRUCT(VkWin32SurfaceCreateInfoKHR);
            surfaceCreateInfo.flags = 0; // flags must be 0
            surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
            surfaceCreateInfo.hwnd = static_cast<HWND>(surfaceInfo.nativeHandle);
            NRI_VK_CHECK_RESULT(vkCreateWin32SurfaceKHR(m_instance->GetNativeHandle(), &surfaceCreateInfo, nullptr, &m_nativeHandle), 
                "Failed to create Vulkan win32 surface");
        }
#else
        WARP_ASSERT(surfaceInfo.type != ESurfaceType::Win32,
            "Vulkan NRI surface type ESurfaceType::Win32 is not supported on this platform!");
#endif
        WARP_ASSERT(GetNativeHandle() != VK_NULL_HANDLE, "Vulkan surface was not created properly!");
    }

    NriSurface::~NriSurface()
    {
        vkDestroySurfaceKHR(m_instance->GetNativeHandle(), this->GetNativeHandle(), nullptr);
    }

} // Warp::nri::vk namespace