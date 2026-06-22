#pragma once

#include "common/attributes.h"
#include "common/memory/arc.h"
#include "nri_vk_common.h"
#include "nri_vk_physical_device.h"

#include <SDL3/SDL_vulkan.h>

#include <string_view>
#include <vector>

namespace Warp::nri::vk
{

struct NriInstanceCreateInfo
{
    NRI_MARK_OPTIONAL(std::string_view) appName;
    NRI_MARK_OPTIONAL(std::string_view) engineName;
    NRI_MARK_OPTIONAL(uint32_t appVersion) = VK_MAKE_API_VERSION(1, 0, 0, 0);
    NRI_MARK_OPTIONAL(uint32_t engineVersion) = VK_MAKE_API_VERSION(1, 0, 0, 0);
    SDL_Window* window = nullptr;
};
class NriInstance : public ArcMark<NriInstance>
{
public:
    NriInstance() = default;
    NriInstance(const NriInstance&) = delete;
    NriInstance& operator=(const NriInstance&) = delete;
    ~NriInstance();

    bool Init(const NriInstanceCreateInfo& createInfo) noexcept;

    inline constexpr EApiVersion GetApiVersion() const noexcept { return m_apiVersion; }
    inline constexpr VkInstance GetNativeHandle() const noexcept { return m_nativeHandle; }

    /// @brief Returns an array of all available physical devices for this NriInstance.
    ///
    /// Note: previously VkSurfaceKHR could be passed to filter physical devices that support presentation to that
    /// surface. This was removed to simplify API. Now in order to check for surface support, user must query
    /// NriPhysicalDeviceSurfaceProperties for each physical device and check whether required surface formats and
    /// present modes are available.
    WARP_NODISCARD("Array of Arc handles is being allocated on return")
    std::vector<Arc<NriPhysicalDevice>> QueryAvailablePhysicalDevices();

private:
    std::vector<const char*> GetInstanceLayersArray() const noexcept;
    std::vector<const char*> GetInstanceExtensionsArray(SDL_Window* window) const noexcept;
    VkDebugUtilsMessengerCreateInfoEXT GetValidationLayerMessengerCreateInfo();
    void InitValidationLayerMessenger();

    EApiVersion m_apiVersion = EApiVersion::Vk_1_0;
    VkInstance m_nativeHandle = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_validationLayerMessenger = VK_NULL_HANDLE;
};

} // namespace Warp::nri::vk
