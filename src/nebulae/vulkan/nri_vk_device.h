#pragma once

#include "nri_vk_instance.h"
#include "nri_vk_surface.h"

#include "common/memory/arc.h"

#include <array>
#include <string_view>

namespace Warp::nri::vk
{

    enum class EDeviceQueueType
    {
        /// Supports graphics, compute and transfer operations
        Universal,
        /// Supports fast compute operations (occasionally might support presenting to surface)
        Compute,
        /// Supports fast DMA/transfers
        Transfer,
        NumTypes,
    };

    /// @brief Represents a physical device that is suitable for creating NRI logical device
    /// It stores a pointer to physical device handle as well as queue family information for each NRI device queue type
    struct NriSuitablePhysicalDeviceInfo
    {
        Arc<NriPhysicalDevice> physicalDevice;

        /// Each queue family info represents a respective queue family to be used during creation of that queue type
        std::array<NriPhysicalDeviceQueueFamilyInformation, EnumValue(EDeviceQueueType::NumTypes)> queueFamilyInfos;
    };

    struct NriDeviceInfo
    {
        Arc<NriInstance> instance;

        /// Optionally, by providing surface during device creation, 
        /// only physical devices that support presentation to this surface will be considered suitable for device creation.
        NRI_MARK_OPTIONAL(Arc<NriSurface>) surface;

        bool bSwapchainRequired = true;
    };

    class NriDevice : public ArcMark<NriDevice>
    {
    public:
        NriDevice() = default;
        NriDevice(const NriDeviceInfo& deviceInfo);

        NriDevice(const NriDevice&) = delete;
        NriDevice& operator=(const NriDevice&) = delete;

        ~NriDevice();

        inline constexpr VkDevice GetNativeHandle() const noexcept { return m_nativeHandle; }
        inline constexpr VkQueue GetQueue(EDeviceQueueType deviceQueueType) const noexcept { return m_deviceQueues.at(EnumValue(deviceQueueType)); }

        inline Arc<NriPhysicalDevice> GetPhysicalDevice() const noexcept { return m_physicalDevice; }

    private:
        struct SuitableDeviceQueryInfo
        {
            Arc<NriSurface> surface;
            std::span<const char* const> requiredDeviceExtensions;
        };
        /// @brief Queries all physical devices available in NRI instance provided during NRI device creation.
        /// From all available physical devices, only those that support required extensions and provide suitable queue families are considered.
        std::vector<NriSuitablePhysicalDeviceInfo> QueryAllSuitableDevices(const SuitableDeviceQueryInfo& queryInfo);

        /// @brief Selects best physical device from all suitable devices (queried from NriDevice::QueryAllSuitableDevices).
        /// Best device is determined by evaluating its properties such as available memory, device type, etc.
        NriSuitablePhysicalDeviceInfo SelectBestSuitableDevice(std::span<const NriSuitablePhysicalDeviceInfo> allSuitablePhysicalDeviceInfos);

        VkDevice m_nativeHandle = VK_NULL_HANDLE;
        Arc<NriInstance> m_instance;
        Arc<NriPhysicalDevice> m_physicalDevice; /// The physical device used to create this logical device

        std::array<VkQueue, EnumValue(EDeviceQueueType::NumTypes)> m_deviceQueues;
    };

} // Warp::nri::vk namespace