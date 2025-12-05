#include "nri_vk_swapchain.h"

namespace Warp::nri::vk
{

    NriSwapchain::NriSwapchain(const NriSwapchainInfo& swapchainInfo)
        : m_device(swapchainInfo.device)
        , m_surface(swapchainInfo.surface)
    {
        WARP_ASSERT(m_device != nullptr, "Invalid NRI device provided during swapchain creation");
        
        Arc<NriPhysicalDevice> physicalDevice = m_device->GetPhysicalDevice();
        WARP_ASSERT(physicalDevice != nullptr && physicalDevice->GetNativeHandle() != VK_NULL_HANDLE, 
            "Invalid NRI physical device provided during swapchain creation");

        WARP_ASSERT(swapchainInfo.numSwapchainImages > 0, "Number of swapchain images must be greater than 0");

        NriPhysicalDeviceSurfaceProperties surfaceProperties = physicalDevice->QuerySurfaceProperties(m_surface->GetNativeHandle());

        VkSurfaceFormatKHR bestSurfaceFormat = QueryBestSurfaceFormat(surfaceProperties.availableSurfaceFormats);
        VkPresentModeKHR bestPresentMode = QueryBestPresentMode(surfaceProperties.availablePresentModes);

        auto createInfo = NRI_VK_STRUCT(VkSwapchainCreateInfoKHR);
        createInfo.flags = 0;
        // Warp's surface propagation ensures that physical device surface properties are supported by logical device.
        // This surface is created during NRI instance creation and passed down to physical device and eventually gets to the swapchain here
        createInfo.surface = m_surface->GetNativeHandle();
        createInfo.minImageCount = swapchainInfo.numSwapchainImages;
        createInfo.imageFormat = bestSurfaceFormat.format;
        createInfo.imageColorSpace = bestSurfaceFormat.colorSpace;
        // TODO: Support more flexible extents (currently using minExtent as default)
        // on Windows minExtent == maxExtent
        WARP_ASSERT(swapchainInfo.width >= surfaceProperties.minExtent.width && swapchainInfo.width <= surfaceProperties.maxExtent.width, 
            "Width is out of min/max bounds for this surface");
        WARP_ASSERT(swapchainInfo.height >= surfaceProperties.minExtent.height && swapchainInfo.height <= surfaceProperties.maxExtent.height,
            "Height is out of min/max bounds for this surface");
        createInfo.imageExtent = VkExtent2D{ .width = swapchainInfo.width, .height = swapchainInfo.height };
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = surfaceProperties.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        WARP_ASSERT(createInfo.imageSharingMode != VK_SHARING_MODE_CONCURRENT, "Concurrent sharing mode is not supported yet");
        {
            // As long as concurrent sharing mode is not used for swapchain - dont specify queue families
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }
        createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = bestPresentMode;
        // specifies whether the Vulkan implementation is allowed to discard rendering operations that affect regions of the surface that are not visible.
        // Warp should set this value to VK_TRUE if we do not expect to read back the content of presentable images before presenting them or after reacquiring them
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;
        
        NRI_VK_CHECK_RESULT(vkCreateSwapchainKHR(m_device->GetNativeHandle(), &createInfo, nullptr, &m_nativeHandle), 
            "Failed to create Vulkan swapchain handle");
    }

    NriSwapchain::~NriSwapchain()
    {
        vkDestroySwapchainKHR(m_device->GetNativeHandle(), GetNativeHandle(), nullptr);
    }

    VkSurfaceFormatKHR NriSwapchain::QueryBestSurfaceFormat(std::span<const VkSurfaceFormatKHR> surfaceFormatArray) const noexcept
    {
        WARP_ASSERT(surfaceFormatArray.size() > 0, "No available surface formats to choose from");
        for (VkSurfaceFormatKHR surfaceFormat : surfaceFormatArray)
        {
            if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return surfaceFormat;
        }
        // TODO: If naive selection fails then we could start ranking the available formats based on how relevant they are
        return surfaceFormatArray[0];
    }

    VkPresentModeKHR NriSwapchain::QueryBestPresentMode(std::span<const VkPresentModeKHR> presentModeArray) const noexcept
    {
        WARP_ASSERT(presentModeArray.size() > 0, "No available present modes to choose from");
        for (VkPresentModeKHR presentMode : presentModeArray)
        {
            // Mailbox generally considered the best performing present mode if available
            // It can be considered similar to triple buffering, although the existence of three buffers alone does not necessarily 
            // mean that the framerate is unlocked
            //
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
                return presentMode;
        }
        // If couldnt find MAILBOT or energy consumption is of priority then fallback to VK_PRESENT_MODE_FIFO_KHR
        // TODO: Check whether FIFO is better than MAILBOX for a device
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D NriSwapchain::QuerySwapchainExtent(const NriPhysicalDeviceSurfaceProperties& surfaceProperties)
    {
        if (surfaceProperties.currentExtent.width == UINT_MAX &&
            surfaceProperties.currentExtent.height == UINT_MAX)
        {
            // If current extent is set to the special value (0xFFFFFFFF, 0xFFFFFFFF) this means that
            // surface extent will be determined by the extent of a swapchain targeting the surface
        }
        else
        {
            // If current extent is a valid value and is not equal to (0xFFFFFFFF, 0xFFFFFFFF) then
            // we can immediately return it
            return surfaceProperties.currentExtent;
        }
        return VkExtent2D();
    }

} // Warp::nri::vk namespace