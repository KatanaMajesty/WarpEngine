#include "nri_vk_swapchain.h"

namespace Warp::nri::vk
{

    NriSwapchain::NriSwapchain(const NriSwapchainInfo& swapchainInfo)
        : m_device(swapchainInfo.device)
        , m_surface(swapchainInfo.surface)
        , m_numSwapchainImages(swapchainInfo.numSwapchainImages)
    {
        WARP_ASSERT(m_device != nullptr, "Invalid NRI device provided during swapchain creation");
        
        Arc<NriPhysicalDevice> physicalDevice = m_device->GetPhysicalDevice();
        WARP_ASSERT(physicalDevice != nullptr && physicalDevice->GetNativeHandle() != VK_NULL_HANDLE, 
            "Invalid NRI physical device provided during swapchain creation");

        WARP_ASSERT(swapchainInfo.numSwapchainImages > 0, "Number of swapchain images must be greater than 0");

        NriPhysicalDeviceSurfaceProperties surfaceProperties = physicalDevice->QuerySurfaceProperties(m_surface->GetNativeHandle());

        m_surfaceFormat = QueryBestSurfaceFormat(surfaceProperties.availableSurfaceFormats);
        VkPresentModeKHR bestPresentMode = QueryBestPresentMode(surfaceProperties.availablePresentModes);

        auto createInfo = NRI_VK_STRUCT(VkSwapchainCreateInfoKHR);
        createInfo.flags = 0;
        // Warp's surface propagation ensures that physical device surface properties are supported by logical device.
        // This surface is created during NRI instance creation and passed down to physical device and eventually gets to the swapchain here
        createInfo.surface = m_surface->GetNativeHandle();
        createInfo.minImageCount = swapchainInfo.numSwapchainImages;
        createInfo.imageFormat = m_surfaceFormat.format;
        createInfo.imageColorSpace = m_surfaceFormat.colorSpace;
        // TODO: Support more flexible extents (currently using minExtent as default)
        // on Windows minExtent == maxExtent
        createInfo.imageExtent = surfaceProperties.currentExtent;
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

        this->CreateSwapchainImages();
        this->CreateSwapchainImageViews();
    }

    NriSwapchain::~NriSwapchain()
    {
        for (VkImageView swapchainImageView : m_swapchainImageViewArray)
        {
            vkDestroyImageView(m_device->GetNativeHandle(), swapchainImageView, nullptr);
        }
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

    VkExtent2D NriSwapchain::QuerySwapchainExtent(const NriPhysicalDeviceSurfaceProperties& surfaceProperties) noexcept
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

    void NriSwapchain::CreateSwapchainImages() noexcept
    {
        WARP_ASSERT(this->GetNumSwapchainImages() > 0, "Invalid number of swapchain images");

        uint32_t khrSwapchainImageCount = 0;
        NRI_VK_CHECK_RESULT(vkGetSwapchainImagesKHR(m_device->GetNativeHandle(), this->GetNativeHandle(), &khrSwapchainImageCount, nullptr), 
            "Failed to get swapchain image count");
        WARP_ASSERT(khrSwapchainImageCount >= this->GetNumSwapchainImages(), "Swapchain image count mismatch");

        m_swapchainImageArray.resize(khrSwapchainImageCount);
        NRI_VK_CHECK_RESULT(vkGetSwapchainImagesKHR(m_device->GetNativeHandle(), this->GetNativeHandle(), &khrSwapchainImageCount, m_swapchainImageArray.data()),
            "Failed to get swapchain images");
    }

    void NriSwapchain::CreateSwapchainImageViews() noexcept
    {
        uint32_t numSwapchainImages = this->GetNumSwapchainImages();
        WARP_ASSERT(numSwapchainImages > 0, "Invalid number of swapchain images");

        m_swapchainImageViewArray.resize(numSwapchainImages);
        for (uint32_t imageIndex = 0; imageIndex < numSwapchainImages; ++imageIndex)
        {
            auto viewCreateInfo = NRI_VK_STRUCT(VkImageViewCreateInfo);
            viewCreateInfo.flags = 0;
            viewCreateInfo.image = this->GetSwapchainImage(imageIndex);
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewCreateInfo.format = this->GetSurfaceFormat().format;
            viewCreateInfo.components = IdentityComponentMapping;
            viewCreateInfo.subresourceRange = VkImageSubresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            };
            NRI_VK_CHECK_RESULT(vkCreateImageView(m_device->GetNativeHandle(), &viewCreateInfo, nullptr, &m_swapchainImageViewArray[imageIndex]), 
                "Failed to create swapchain image view at index {}", imageIndex);
        }
    }

} // Warp::nri::vk namespace