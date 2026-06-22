#pragma once

#include "common/memory/arc.h"
#include "nri_vk_common.h"
#include "nri_vk_device.h"

#include <expected>
#include <vector>

namespace Warp::nri::vk
{

struct NriSwapchainCreateInfo
{
    Arc<NriDevice> device;
    uint32_t numSwapchainImages = 0;
};
class NriSwapchain : public ArcMark<NriSwapchain>
{
public:
    NriSwapchain() = default;
    NriSwapchain(const NriSwapchain&) = delete;
    NriSwapchain& operator=(const NriSwapchain&) = delete;
    ~NriSwapchain();

    bool Init(const NriSwapchainCreateInfo& createInfo) noexcept;

    /// @brief Requests next image from a swapchain. Current swapchain image index can be obtained with
    ///
    /// Swapchain might become incompatible in such cases:
    /// VK_ERROR_OUT_OF_DATE_KHR: The swapchain has become incompatible with the surface and can no longer be used for
    /// rendering. Usually happens after a window resize In such case we need to immediately handle swapchain resize and
    /// bail out of this frame render
    ///
    /// VK_SUBOPTIMAL_KHR: The swapchain can still be used to successfully present to the surface, but the surface
    /// properties are no longer matched exactly
    ///
    /// @returns true if successfully acquired next available image, or false if swapchain properties are out of
    /// date/incompatible.
    bool AcquireNextImage(VkSemaphore signalSemaphore = VK_NULL_HANDLE,
                          VkFence signalFence = VK_NULL_HANDLE,
                          uint64_t timeout = UINT64_MAX) noexcept;

    /// @brief When an application's window is resized internal's of a swapchain need to be resized.
    /// When this happens NriSwapchain::HandleResize must be called in order to properly recreate underlying swapchain
    /// handles. Important note: all outstanding synchronization for NriDevice work must be done before a call to
    /// NriSwapchain::HandleResize
    void HandleResize() noexcept;

    inline constexpr VkSwapchainKHR GetNativeHandle() const noexcept { return m_nativeHandle; }
    inline constexpr VkSurfaceFormatKHR GetSurfaceFormat() const noexcept { return m_surfaceFormat; }
    /// @brief Obtains extent currently used by this swapchain handle
    inline constexpr VkExtent2D GetCurrentExtent() const noexcept { return m_extent; }
    inline constexpr uint32_t GetWidth() const noexcept { return GetCurrentExtent().width; }
    inline constexpr uint32_t GetHeight() const noexcept { return GetCurrentExtent().height; }

    /// @brief Returns the number of swapchain images requested by the client (specified as
    /// NriSwapchainInfo::numSwapchainImages)
    inline constexpr uint32_t GetNumSwapchainImages() const noexcept { return m_numSwapchainImages; }

    /// @returns index of an image that can currently be used from this swapchain for present operations.
    /// To properly obtain next index after presenting NriSwapchain::AcquireNextImage must be called beforehand
    inline constexpr uint32_t GetCurrentImageIndex() const noexcept { return m_currentImageIndex; }

    inline constexpr VkImage GetSwapchainImage(uint32_t imageIndex) const
    {
        WARP_ASSERT(imageIndex < this->GetNumSwapchainImages(), "OOB swapchain image index");
        return m_swapchainImageArray[imageIndex];
    }

    inline constexpr VkImageView GetSwapchainImageView(uint32_t imageIndex) const
    {
        WARP_ASSERT(imageIndex < this->GetNumSwapchainImages(), "OOB swapchain image view index");
        return m_swapchainImageViewArray[imageIndex];
    }

private:
    /// Releases all internal Vulkan swapchain handles an associated images
    void CleanupSwapchainResources() noexcept;

    /// Creates internal Vulkan swapchain handle
    void CreateNativeSwapchain(const NriPhysicalDeviceSurfaceProperties& surfaceProperties) noexcept;

    /// Based on provided NriPhysicalDeviceSurfaceProperties queries best properties to be used for a swapchain.
    /// Such properties include but not limited to present mode, surface format, swapchain extent, etc.
    ///
    /// This should only be invoked from NriSwapchain::CreateNativeSwapchain
    void QueryNativeSwapchainProperties(const NriPhysicalDeviceSurfaceProperties& surfaceProperties) noexcept;

    /// Creates Vulkan images and image views to be associated with this swapchain object
    void CreateNativeSwapchainImagesAndViews() noexcept;

    Arc<NriDevice> m_device;   // NRI device that was used to create this swapchain

    /// @brief This number corresponds to amount of swapchain images requested by client.
    /// Some implementations might actually allocate more images, but only first N will be exposed by this NRI handle
    /// (where N is the number of images requested by client).
    uint32_t m_numSwapchainImages = 0;
    uint32_t m_currentImageIndex = 0;

    VkExtent2D m_extent = VkExtent2D();
    VkSurfaceFormatKHR m_surfaceFormat = VkSurfaceFormatKHR();
    VkPresentModeKHR m_presentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;

    VkSwapchainKHR m_nativeHandle = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchainImageArray;
    std::vector<VkImageView> m_swapchainImageViewArray; // size always equals to m_swapchainImageArray::size
};

} // namespace Warp::nri::vk