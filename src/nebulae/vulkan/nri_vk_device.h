#pragma once

#include "nri_vk_common.h"
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
        /// Should always be the last one!
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
        inline Arc<NriPhysicalDevice> GetPhysicalDevice() const noexcept { return m_physicalDevice; }

        inline constexpr const NriPhysicalDeviceQueueFamilyInformation& GetQueueFamilyInfo(EDeviceQueueType deviceQueueType) const noexcept
        {
            return m_deviceQueueFamilyInfos.at(EnumValue(deviceQueueType));
        }
        inline constexpr VkQueue GetQueue(EDeviceQueueType deviceQueueType) const noexcept { return m_deviceQueues.at(EnumValue(deviceQueueType)); }
        inline constexpr VkCommandPool GetCommandPool(EDeviceQueueType deviceQueueType) const noexcept { return m_commandPools.at(EnumValue(deviceQueueType)); }
        inline constexpr VmaAllocator GetMemoryAllocator() const noexcept { return m_memoryAllocator; }

        /// @brief Waits on host for all outstanding device work to finish
        inline void WaitIdle() noexcept { NRI_VK_CHECK_RESULT(vkDeviceWaitIdle(GetNativeHandle()), "Failed to wait for device to finish work"); }

        void BeginDebugLabel(VkCommandBuffer commandBuffer, std::string_view labelName, glm::vec4 labelColor = glm::vec4(1.0f)) const noexcept;
        void EndDebugLabel(VkCommandBuffer commandBuffer) const noexcept;

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

        // clang-format off
        static constexpr uint32_t NumQueueTypes = EnumValue(EDeviceQueueType::NumTypes);
        std::array<NriPhysicalDeviceQueueFamilyInformation, NumQueueTypes> m_deviceQueueFamilyInfos;
        std::array<VkQueue,                                 NumQueueTypes> m_deviceQueues;
        std::array<VkCommandPool,                           NumQueueTypes> m_commandPools;
        // clang-format on

        VmaAllocator m_memoryAllocator = VK_NULL_HANDLE; /// VMA allocator for this device, initialized on device creation and destroyed on device destruction
    };

} // Warp::nri::vk namespace