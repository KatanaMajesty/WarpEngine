#include "nri_vk_swapchain.h"

namespace Warp::nri::vk
{

    NriSwapchain::NriSwapchain(const NriSwapchainInfo& swapchainInfo)
        : m_device(swapchainInfo.device)
    {
        WARP_ASSERT(m_device != nullptr, "Invalid NRI device provided during swapchain creation");
        
        Arc<NriPhysicalDevice> physicalDevice = m_device->GetPhysicalDevice();
        WARP_ASSERT(physicalDevice != nullptr && physicalDevice->GetNativeHandle() != VK_NULL_HANDLE, 
            "Invalid NRI physical device provided during swapchain creation");

        WARP_ASSERT(swapchainInfo.numSwapchainImages > 0, "Number of swapchain images must be greater than 0");

        auto createInfo = NRI_VK_STRUCT(VkSwapchainCreateInfoKHR);
        createInfo.flags = 0;
        // Warp's surface propagation ensures that physical device surface properties are supported by logical device.
        // This surface is created during NRI instance creation and passed down to physical device and eventually gets to the swapchain here
        createInfo.surface = physicalDevice->GetSurface();
        createInfo.minImageCount = swapchainInfo.numSwapchainImages;
        
        vkCreateSwapchainKHR(
            m_device->GetNativeHandle(),
            nullptr,
            nullptr,
            &m_nativeHandle);
    }

} // Warp::nri::vk namespace