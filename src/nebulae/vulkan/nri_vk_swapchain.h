#pragma once

#include "nri_vk_common.h"
#include "nri_vk_device.h"
#include "nri_vk_surface.h"

#include "common/memory/arc.h"
#include "common/memory/arc_object.h"

#include <vector>

namespace Warp::nri::vk
{

    struct NriSwapchainInfo
    {
        Arc<NriSurface> surface;
        Arc<NriDevice> device;

        uint32_t numSwapchainImages = 0;
    };

    class NriSwapchain : public AtomicallyRefCounted<NriSwapchain>
    {
    public:
        NriSwapchain() = delete;
        NriSwapchain(const NriSwapchainInfo& swapchainInfo);

        NriSwapchain(const NriSwapchain&) = delete;
        NriSwapchain& operator=(const NriSwapchain&) = delete;

    private:
        VkSurfaceFormatKHR QueryBestSurfaceFormat(std::span<const VkSurfaceFormatKHR> surfaceFormatArray);
        VkPresentModeKHR QueryBestPresentMode(std::span<const VkPresentModeKHR> presentModeArray);

        VkSwapchainKHR m_nativeHandle = VK_NULL_HANDLE;
        Arc<NriDevice> m_device; /// NRI device that was used to create this swapchain
        Arc<NriSurface> m_surface; /// NRI surface associated with this swapchain
    };

} // Warp::nri::vk namespace