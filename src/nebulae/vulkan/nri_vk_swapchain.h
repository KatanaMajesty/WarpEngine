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
        uint16_t width = 0;
        uint16_t height = 0;
    };

    class NriSwapchain : public AtomicallyRefCounted<NriSwapchain>
    {
    public:
        NriSwapchain() = delete;
        NriSwapchain(const NriSwapchainInfo& swapchainInfo);

        NriSwapchain(const NriSwapchain&) = delete;
        NriSwapchain& operator=(const NriSwapchain&) = delete;

        ~NriSwapchain();

        inline constexpr VkSwapchainKHR GetNativeHandle() const noexcept { return m_nativeHandle; }

    private:
        VkSurfaceFormatKHR QueryBestSurfaceFormat(std::span<const VkSurfaceFormatKHR> surfaceFormatArray) const noexcept;
        VkPresentModeKHR QueryBestPresentMode(std::span<const VkPresentModeKHR> presentModeArray) const noexcept;

        /// @brief Swapchain extent query is based of NriPhysicalDeviceSurfaceProperties that can be queried from NriPhysicalDevice
        VkExtent2D QuerySwapchainExtent(const NriPhysicalDeviceSurfaceProperties& surfaceProperties);

        VkSwapchainKHR m_nativeHandle = VK_NULL_HANDLE;
        Arc<NriDevice> m_device; /// NRI device that was used to create this swapchain
        Arc<NriSurface> m_surface; /// NRI surface associated with this swapchain
    };

} // Warp::nri::vk namespace