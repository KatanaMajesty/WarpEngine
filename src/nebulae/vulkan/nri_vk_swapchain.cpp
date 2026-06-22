#include "nri_vk_swapchain.h"

namespace Warp::nri::vk
{

NriSwapchain::~NriSwapchain() { CleanupSwapchainResources(); }

bool NriSwapchain::Init(const NriSwapchainCreateInfo& createInfo) noexcept
{
    m_device = createInfo.device;
    m_numSwapchainImages = createInfo.numSwapchainImages;
    WARP_ASSERT(m_device != nullptr, "Invalid NRI device provided during swapchain creation");

    Arc<NriPhysicalDevice> physicalDevice = m_device->GetPhysicalDevice();
    WARP_ASSERT(m_numSwapchainImages > 0, "Number of swapchain images must be greater than 0");
    WARP_ASSERT(physicalDevice != nullptr && physicalDevice->GetNativeHandle() != VK_NULL_HANDLE,
                "Invalid NRI physical device provided during swapchain creation");

    NriPhysicalDeviceSurfaceProperties surfaceProperties =
        physicalDevice->QuerySurfaceProperties(m_device->GetSurface());
    CreateNativeSwapchain(surfaceProperties);
    CreateNativeSwapchainImagesAndViews();
    return true;
}

bool NriSwapchain::AcquireNextImage(VkSemaphore signalSemaphore, VkFence signalFence, uint64_t timeout) noexcept
{
    m_currentImageIndex = UINT_MAX;
    VkResult result = vkAcquireNextImageKHR(m_device->GetNativeHandle(), GetNativeHandle(), timeout, signalSemaphore,
                                            signalFence, &m_currentImageIndex);
    switch (result)
    {
        // The swapchain has become incompatible with the surface and can no longer be used for rendering. Usually
        // happens after a window resize In such case we need to immediately handle swapchain resize and bail out of
        // this frame render
        case VK_ERROR_OUT_OF_DATE_KHR: WARP_FALLTHROUGH;
        // The swapchain can still be used to successfully present to the surface, but the surface properties are no
        // longer matched exactly
        case VK_SUBOPTIMAL_KHR: return false;
        default: NRI_VK_CHECK_RESULT(result, "Failed to acquire a swapchain image");
    }
    return true;
}

void NriSwapchain::HandleResize() noexcept
{
    WARP_ASSERT(m_device != nullptr, "Invalid NRI device provided during swapchain creation");
    CleanupSwapchainResources();

    Arc<NriPhysicalDevice> physicalDevice = m_device->GetPhysicalDevice();
    WARP_ASSERT(physicalDevice != nullptr && physicalDevice->GetNativeHandle() != VK_NULL_HANDLE,
                "Invalid NRI physical device provided during swapchain creation");

    NriPhysicalDeviceSurfaceProperties surfaceProperties =
        physicalDevice->QuerySurfaceProperties(m_device->GetSurface());
    CreateNativeSwapchain(surfaceProperties);
    CreateNativeSwapchainImagesAndViews();
}

void NriSwapchain::CleanupSwapchainResources() noexcept
{
    for (VkImageView swapchainImageView : m_swapchainImageViewArray)
    {
        vkDestroyImageView(m_device->GetNativeHandle(), swapchainImageView, nullptr);
    }
    vkDestroySwapchainKHR(m_device->GetNativeHandle(), GetNativeHandle(), nullptr);
}

void NriSwapchain::CreateNativeSwapchain(const NriPhysicalDeviceSurfaceProperties& surfaceProperties) noexcept
{
    // we assume that m_numSwapchainImages is already set at this point
    WARP_ASSERT(m_numSwapchainImages != 0, "Number of swapchain images is not properly set!");
    WARP_ASSERT(m_numSwapchainImages >= surfaceProperties.minImageCount &&
                    m_numSwapchainImages <= surfaceProperties.maxImageCount,
                "Number of swapchain images is not supported!");

    this->QueryNativeSwapchainProperties(surfaceProperties);

    auto createInfo = NRI_VK_STRUCT(VkSwapchainCreateInfoKHR);
    createInfo.flags = 0;
    // Warp's surface propagation ensures that physical device surface properties are supported by logical device.
    // This surface is created during NRI instance creation and passed down to physical device and eventually gets to
    // the swapchain here
    createInfo.surface = m_device->GetSurface();
    createInfo.minImageCount = m_numSwapchainImages;
    createInfo.imageFormat = m_surfaceFormat.format;
    createInfo.imageColorSpace = m_surfaceFormat.colorSpace;
    // TODO: Support more flexible extents (currently using minExtent as default)
    // on Windows minExtent == maxExtent
    createInfo.imageExtent = m_extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = surfaceProperties.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    WARP_ASSERT(createInfo.imageSharingMode != VK_SHARING_MODE_CONCURRENT,
                "Concurrent sharing mode is not supported yet");
    {
        // As long as concurrent sharing mode is not used for swapchain - dont specify queue families
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = m_presentMode;
    // specifies whether the Vulkan implementation is allowed to discard rendering operations that affect regions of the
    // surface that are not visible. Warp should set this value to VK_TRUE if we do not expect to read back the content
    // of presentable images before presenting them or after reacquiring them
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    NRI_VK_CHECK_RESULT(vkCreateSwapchainKHR(m_device->GetNativeHandle(), &createInfo, nullptr, &m_nativeHandle),
                        "Failed to create Vulkan swapchain handle");
}

void NriSwapchain::QueryNativeSwapchainProperties(const NriPhysicalDeviceSurfaceProperties& surfaceProperties) noexcept
{
    static auto GetBestSwapchainExtent = [](const NriPhysicalDeviceSurfaceProperties& surfaceProperties) -> VkExtent2D
    {
        if (surfaceProperties.currentExtent.width == UINT_MAX && surfaceProperties.currentExtent.height == UINT_MAX)
        {
            // If current extent is set to the special value (0xFFFFFFFF, 0xFFFFFFFF) this means that
            // surface extent will be determined by the extent of a swapchain targeting the surface
            WARP_ASSERT(false, "Unimplemented yet");
        }
        else
        {
            // If current extent is a valid value and is not equal to (0xFFFFFFFF, 0xFFFFFFFF) then
            // we can immediately return it
            return surfaceProperties.currentExtent;
        }
        return VkExtent2D();
    };
    m_extent = GetBestSwapchainExtent(surfaceProperties);

    static auto GetBestSurfaceFormat =
        [](const NriPhysicalDeviceSurfaceProperties& surfaceProperties) -> VkSurfaceFormatKHR
    {
        WARP_ASSERT(!surfaceProperties.availableSurfaceFormats.empty(), "No available surface formats to choose from");
        for (VkSurfaceFormatKHR surfaceFormat : surfaceProperties.availableSurfaceFormats)
        {
            if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return surfaceFormat;
        }
        // TODO: If naive selection fails then we could start ranking the available formats based on how relevant they
        // are
        return surfaceProperties.availableSurfaceFormats[0];
    };
    m_surfaceFormat = GetBestSurfaceFormat(surfaceProperties);

    static auto GetBestPresentMode = [](const NriPhysicalDeviceSurfaceProperties& surfaceProperties) -> VkPresentModeKHR
    {
        WARP_ASSERT(!surfaceProperties.availablePresentModes.empty(), "No available present modes to choose from");
        for (VkPresentModeKHR presentMode : surfaceProperties.availablePresentModes)
        {
            // Mailbox generally considered the best performing present mode if available
            // It can be considered similar to triple buffering, although the existence of three buffers alone does not
            // necessarily mean that the framerate is unlocked
            //
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
                return presentMode;
        }
        // If couldnt find MAILBOT or energy consumption is of priority then fallback to VK_PRESENT_MODE_FIFO_KHR
        // TODO: Check whether FIFO is better than MAILBOX for a device
        return VK_PRESENT_MODE_FIFO_KHR;
    };
    m_presentMode = GetBestPresentMode(surfaceProperties);
}

void NriSwapchain::CreateNativeSwapchainImagesAndViews() noexcept
{
    // assume number of images was already previously set
    WARP_ASSERT(m_numSwapchainImages > 0, "Invalid number of swapchain images");

    // obtain native Vulkan image handles
    uint32_t swapchainImageCount = 0;
    {
        NRI_VK_CHECK_RESULT(
            vkGetSwapchainImagesKHR(m_device->GetNativeHandle(), m_nativeHandle, &swapchainImageCount, nullptr),
            "Failed to get swapchain image count");
        WARP_ASSERT(swapchainImageCount >= m_numSwapchainImages, "Swapchain image count mismatch");

        m_swapchainImageArray.resize(swapchainImageCount);
        NRI_VK_CHECK_RESULT(vkGetSwapchainImagesKHR(m_device->GetNativeHandle(), m_nativeHandle, &swapchainImageCount,
                                                    m_swapchainImageArray.data()),
                            "Failed to get swapchain images");
    }

    // create image views for each Vulkan image handle
    {
        m_swapchainImageViewArray.resize(swapchainImageCount);
        for (uint32_t imageIndex = 0; imageIndex < swapchainImageCount; ++imageIndex)
        {
            auto viewCreateInfo = NRI_VK_STRUCT(VkImageViewCreateInfo);
            viewCreateInfo.flags = 0;
            viewCreateInfo.image = this->GetSwapchainImage(imageIndex);
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewCreateInfo.format = this->GetSurfaceFormat().format;
            viewCreateInfo.components = IdentityComponentMapping;
            viewCreateInfo.subresourceRange = VkImageSubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                      .baseMipLevel = 0,
                                                                      .levelCount = 1,
                                                                      .baseArrayLayer = 0,
                                                                      .layerCount = 1};
            NRI_VK_CHECK_RESULT(vkCreateImageView(m_device->GetNativeHandle(), &viewCreateInfo, nullptr,
                                                  &m_swapchainImageViewArray[imageIndex]),
                                "Failed to create swapchain image view at index {}", imageIndex);
        }
    }
}

} // namespace Warp::nri::vk