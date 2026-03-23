#pragma once

#include "nri_vk_common.h"
#include "nri_vk_physical_device.h"

#include "common/memory/arc.h"
#include "common/attr_defs.h"

#include <string_view>
#include <vector>

namespace Warp::nri::vk
{

    /// @brief A simple wrapper around VkAppliationInfo
    struct NriInstanceInfo
    {
        /// Lifetime of appName underlying string must outlive NriInstance construction process
        std::string_view appName;
        uint32_t appVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);

        /// Lifetime of engineName underlying string must outlive NriInstance construction process
        std::string_view engineName;
        uint32_t engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);

        bool bSurfaceRequired = true;
    };

    class NriInstance : public ArcMark<NriInstance>
    {
    public:
        NriInstance() = default;
        NriInstance(const NriInstanceInfo& instanceInfo);

        NriInstance(const NriInstance&) = delete;
        NriInstance& operator=(const NriInstance&) = delete;

        ~NriInstance();

        inline constexpr EApiVersion GetApiVersion() const noexcept { return m_apiVersion; }
        inline constexpr VkInstance GetNativeHandle() const noexcept { return m_nativeHandle; }

        /// @brief Returns an array of all available physical devices for this NriInstance.
        /// 
        /// Note: previously VkSurfaceKHR could be passed to filter physical devices that support presentation to that surface. This was removed to simplify API.
        /// Now in order to check for surface support, user must query NriPhysicalDeviceSurfaceProperties for each physical device and check whether 
        /// required surface formats and present modes are available.
        WARP_A_NODISCARD("Array of Arc handles is being allocated on return") 
        std::vector<Arc<NriPhysicalDevice>> QueryAvailablePhysicalDevices();

    private:
        VkDebugUtilsMessengerCreateInfoEXT GetValidationLayerMessengerCreateInfo();
        void InitValidationLayerMessenger();

        EApiVersion m_apiVersion = EApiVersion::Vk_1_0;
        VkInstance m_nativeHandle = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_validationLayerMessenger = VK_NULL_HANDLE;
    };

} // Warp::nri::vk namespace
