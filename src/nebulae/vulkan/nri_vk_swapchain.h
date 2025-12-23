#pragma once

#include "nri_vk_common.h"
#include "nri_vk_device.h"
#include "nri_vk_surface.h"

#include "common/memory/arc.h"

#include <vector>

namespace Warp::nri::vk
{

    struct NriSwapchainInfo
    {
        Arc<NriSurface> surface;
        Arc<NriDevice> device;

        uint32_t numSwapchainImages = 0;
        uint16_t width = 0;
        uint16_t height = 0;
    };

    class NriSwapchain : public ArcMark<NriSwapchain>
    {
    public:
        NriSwapchain() = delete;
        NriSwapchain(const NriSwapchainInfo& swapchainInfo);

        NriSwapchain(const NriSwapchain&) = delete;
        NriSwapchain& operator=(const NriSwapchain&) = delete;

        ~NriSwapchain();

        inline constexpr VkSwapchainKHR GetNativeHandle() const noexcept { return m_nativeHandle; }
        inline constexpr VkSurfaceFormatKHR GetSurfaceFormat() const noexcept { return m_surfaceFormat; }

        /// @brief Returns the number of swapchain images requested by the client (specified as NriSwapchainInfo::numSwapchainImages)
        inline constexpr uint32_t GetNumSwapchainImages() const noexcept { return m_numSwapchainImages; }

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
        VkSurfaceFormatKHR QueryBestSurfaceFormat(std::span<const VkSurfaceFormatKHR> surfaceFormatArray) const noexcept;
        VkPresentModeKHR QueryBestPresentMode(std::span<const VkPresentModeKHR> presentModeArray) const noexcept;

        /// @brief Swapchain extent query is based of NriPhysicalDeviceSurfaceProperties that can be queried from NriPhysicalDevice
        VkExtent2D QuerySwapchainExtent(const NriPhysicalDeviceSurfaceProperties& surfaceProperties) noexcept;

        void CreateSwapchainImages() noexcept;
        void CreateSwapchainImageViews() noexcept;

        Arc<NriDevice> m_device; /// NRI device that was used to create this swapchain
        Arc<NriSurface> m_surface; /// NRI surface associated with this swapchain
        VkSwapchainKHR m_nativeHandle = VK_NULL_HANDLE;
        VkSurfaceFormatKHR m_surfaceFormat = VkSurfaceFormatKHR();

        /// @brief This number corresponds to amount of swapchain images requested by client.
        /// Some implementations might actually allocate more images, but only first N will be exposed by this NRI handle
        /// (where N is the number of images requested by client).
        uint32_t m_numSwapchainImages = 0;

        std::vector<VkImage> m_swapchainImageArray;
        std::vector<VkImageView> m_swapchainImageViewArray; // size always equals to m_swapchainImageArray::size
    };

} // Warp::nri::vk namespace