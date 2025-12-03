#pragma once

#include "nri_vk_common.h"
#include "nri_vk_device.h"

#include "common/memory/arc.h"
#include "common/memory/arc_object.h"

#include <vector>

namespace Warp::nri::vk
{

    struct NriSwapchainProperties
    {
        
    };

    struct NriSwapchainInfo
    {
        Arc<NriPhysicalDevice> device;

        uint32_t numSwapchainImages = 0;
        VkFormat imageFormat = VK_FORMAT_UNDEFINED;
        VkColorSpaceKHR imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    };

    class NriSwapchain
    {
    public:
        NriSwapchain(VkDevice nativeDevice, const NriSwapchainInfo& swapchainInfo);


    private:
        VkSwapchainKHR m_nativeHandle = VK_NULL_HANDLE;
        Arc<NriDevice> m_device;
    };

} // Warp::nri::vk namespace