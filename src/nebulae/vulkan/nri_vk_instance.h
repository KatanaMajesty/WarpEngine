#pragma once

#include "generated/nri_pci_vendor_ids.h"
#include "nri_vk_common.h"

#include "common/memory/arc_object.h"
#include "common/memory/arc.h"
#include "common/enum_flag_set.h"

#include <vulkan/vulkan.h>

#include <string_view>
#include <span>
#include <vector>
#include <unordered_set>

namespace Warp::nri::vk
{

    /// @brief enum values from this enumeration represents vendors that are recognized by NRI interface of Warp.
    /// If any other device vendor, that is not recognized, needs to be used, WARP_PCI_VENDOR_ID_* macros can be used instead
    enum class EPhysicalDeviceVendor : uint16_t
    {
        Unknown = 0,
        Nvidia = WARP_PCI_VENDOR_ID_NVIDIA_CORPORATION,
        AMD = WARP_PCI_VENDOR_ID_ADVANCED_MICRO_DEVICES_INC_AMD,
        Intel = WARP_PCI_VENDOR_ID_INTEL_CORPORATION,
    };

    /// @brief Direct enum wrapper over VK_API_VERSION macros. Should be used for Vulkan API versioning instead of direct uint32_t
    enum class EApiVersion : uint32_t
    {
        Vk_1_0 = VK_MAKE_API_VERSION(0, /*major*/ 1, /*minor*/ 0, 0),
        Vk_1_1 = VK_MAKE_API_VERSION(0, /*major*/ 1, /*minor*/ 1, 0),
        Vk_1_2 = VK_MAKE_API_VERSION(0, /*major*/ 1, /*minor*/ 2, 0),
        Vk_1_3 = VK_MAKE_API_VERSION(0, /*major*/ 1, /*minor*/ 3, 0),
        Vk_1_4 = VK_MAKE_API_VERSION(0, /*major*/ 1, /*minor*/ 4, 0),
        MaxSupported = Vk_1_4, // Specifies the latest version
    };

    struct NriPhysicalDeviceInformation
    {
        /// Device's reported name (from VkPhysicalDeviceProperties::deviceName)
        std::string deviceName;

        /// Drivers identification name (from VkPhysicalDeviceDriverProperties::driverName)
        std::string driverName;

        /// Drivers additional information (from VkPhysicalDeviceDriverProperties::driverInfo)
        std::string driverInfo;

        /// Marks the vendor of this physical device. If the vendor is not recognized, EPhysicalDeviceVendor::Unknown is set.
        /// Device vendor is uint16_t defined by PCI-SIG organization, for more information see https://pcisig.com/membership/vendor-id
        /// Device vendors are defined in "nri_pci_vendor_ids.h" auto-generated file
        EPhysicalDeviceVendor vendor = EPhysicalDeviceVendor::Unknown;

        /// Device type as defined in VkPhysicalDeviceProperties::deviceType. 
        /// This would usually be used to determine what physical device to choose when multiple devices are available
        VkPhysicalDeviceType type = VK_PHYSICAL_DEVICE_TYPE_OTHER;

        /// The highest version of Vulkan API supported by this physical device
        EApiVersion supportedApiVersion = EApiVersion::Vk_1_0;
    };

    enum class EMemoryProperty : uint16_t
    {
        /// For reference see VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        DeviceLocal,
        /// For reference see VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        HostVisible,
        /// For reference see VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        HostCoherent,
        /// For reference see VK_MEMORY_PROPERTY_HOST_CACHED_BIT
        HostCached,
        /// For reference see VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT
        LazilyAllocated,
    };

    /// For reference: https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceMemoryProperties.html
    /// this describes VkMemoryHeap + VkMemoryPropertyFlags
    struct NriPhysicalDeviceMemoryHeap
    {
        /// A heap's size is the total memory size in bytes in the heap.
        uint64_t heapTotalSizeInBytes;

        /// Memory properties flag set defines some capabilities of this memory heap, such as device locality or host visibility
        FlagSet<EMemoryProperty> memoryProperties;
    };
    
    struct NriPhysicalDeviceMemoryInformation
    {
        /// Physical device's total size in bytes is a sum of all heap's sizes available for this physical device
        ///
        /// REMARK: Total size in bytes of a physical device would probably only make sense when choosing physical device to create a virtual Vk device,
        ///         otherwise it should not be relied upon when querying available memory for resource/handle creation or any other memory-related behaviour
        uint64_t totalSizeInBytes = 0;

        /// An array of all defined memory heaps for this specific physical device.
        /// For a properly defined physical device this array would not be empty.
        std::vector<NriPhysicalDeviceMemoryHeap> memoryHeaps;
    };

    enum class EQueueCapability : uint16_t
    {
        /// For reference see VK_QUEUE_GRAPHICS_BIT 
        Graphics,
        /// For reference see VK_QUEUE_COMPUTE_BIT 
        Compute,
        /// For reference see VK_QUEUE_TRANSFER_BIT 
        Transfer,
    };

    struct NriPhysicalDeviceQueueFamilyInformation
    {
        inline constexpr bool IsInvalid() const noexcept { return queueFamilyIndex == uint32_t(-1) || queueCount == 0; }

        /// @brief integer indicating the index of the queue family in which to create the queues on this device. 
        /// This index corresponds to the index of an element of the 'pQueueFamilyProperties' array that was returned by 'vkGetPhysicalDeviceQueueFamilyProperties'
        /// 
        /// Remark: this should generally just be passed to VkDeviceQueueCreateInfo on device creation
        /// See https://docs.vulkan.org/refpages/latest/refpages/source/VkDeviceQueueCreateInfo.html
        uint32_t queueFamilyIndex = uint32_t(-1);

        /// Count of queues in this queue family. Each queue family must support at least one queue.
        uint32_t queueCount = 0;
        
        /// @brief Represents capabilities of this queue family queues.
        FlagSet<EQueueCapability> capabilities;

        /// @brief Indicates whether this queue family supports presentation to a surface specified during this physical device NRI handle creation.
        /// Surface relating to this physical device can be obtained by calling NriPhysicalDevice::GetSurface().
        /// Most likely this surface would have been provided during NriInstance creation, specified by NriInstanceInfo::nativeWindowHandle.
        bool surfaceSupport = false;
    };

    struct NriPhysicalDeviceSurfaceProperties
    {
        uint32_t minImageCount = 0;

        /// Note: Formulas such as min(N, maxImageCount) are not correct, since maxImageCount may be zero.
        uint32_t maxImageCount = 0;
        
        /// Regarding extents: On Windows platform the output surface must always be equal to the paintable window size, so min/max/curr extents will be equal.
        VkExtent2D minExtent = VkExtent2D();

        /// On some platforms, it is normal that maxImageExtent may become (0, 0), for example when the window is minimized. 
        /// In such a case, it is not possible to create a swapchain due to the Valid Usage requirements, unless scaling is selected 
        /// through VkSwapchainPresentScalingCreateInfoKHR, if supported.
        /// from https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainCreateInfoKHR.html
        VkExtent2D maxExtent = VkExtent2D();

        /// @brief a bitmask of VkImageUsageFlagBits representing the ways the application can use the presentable
        /// images of a swapchain created with VkPresentModeKHR set to:
        ///     VK_PRESENT_MODE_FIFO_LATEST_READY_KHR,
        ///     VK_PRESENT_MODE_IMMEDIATE_KHR,
        ///     VK_PRESENT_MODE_MAILBOX_KHR,
        ///     VK_PRESENT_MODE_FIFO_KHR or
        ///     VK_PRESENT_MODE_FIFO_RELAXED_KHR for the surface on the specified device.
        /// VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT must be included in the set. Implementations may support additional usages.
        VkImageUsageFlags supportedUsageFlags = VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM;

        std::vector<VkSurfaceFormat2KHR> availableSurfaceFormats;
        std::vector<VkPresentModeKHR> availablePresentModes;
    };

    class NriPhysicalDevice : public AtomicallyRefCounted<NriPhysicalDevice>
    {
    public:
        NriPhysicalDevice() = default;

        /// @brief Constructs physical device wrapper over native physical defvice handle provided as a parameter.
        /// This constructor is kept private so that only NriInstance is able to allocate physical devices.
        /// Additionally, VkSurfaceKHR is required to properly query physical device capabilities over surface presentation.
        NriPhysicalDevice(VkPhysicalDevice nativeHandle, VkSurfaceKHR surface);

        NriPhysicalDevice(const NriPhysicalDevice&) = delete;
        NriPhysicalDevice& operator=(const NriPhysicalDevice&) = delete;

        inline constexpr VkPhysicalDevice GetNativeHandle() const noexcept { return m_nativeHandle; }
        inline constexpr VkSurfaceKHR GetSurface() const noexcept { return m_surface; }

        inline bool IsExtensionSupported(std::string_view extensionName) const noexcept { return m_supportedDeviceExtensionLUT.contains(extensionName); }

        inline constexpr const NriPhysicalDeviceInformation& GetInformation() const noexcept { return m_info; }
        inline constexpr const NriPhysicalDeviceMemoryInformation& GetMemoryInformation() const noexcept { return m_memoryInfo; }
        inline constexpr const NriPhysicalDeviceSurfaceProperties& GetSurfaceProperties() const noexcept { return m_surfaceProperties; }
        inline constexpr std::span<const NriPhysicalDeviceQueueFamilyInformation> GetQueueFamilyInfoArray() const noexcept { return m_queueFamilyInfos; }

        /// @brief A shortcut to obtain Vulkan API version supported by this physical device from NriPhysicalDeviceInformation
        inline constexpr EApiVersion GetApiVersion() const noexcept { return GetInformation().supportedApiVersion; }

    private:
        /// @brief Queries all supported and recognized extension support (defined as EDeviceExtension enumeration)
        /// A flagset of all supported recognized extensions is formed and can be accessed by calling NriPhysicalDevice::GetSupportedExtensions
        void QuerySupportedExtensions();

        /// @brief Queries physical device generic information such as driver version/name, device name, supported API version, vendor, etc.
        /// The result is stored in NriPhysicalDeviceInformation 'm_info' field of this object and can be obtained using NriPhysicalDevice::GetInformation()
        /// 
        /// This call will throw a critical error on failure which will abort initialization of physical device NRI handle.
        void QueryInformation();

        /// @brief Queries physical device memory information
        /// The result is stored in NriPhysicalDeviceMemoryInformation field and can be obtained by calling NriPhysicalDevice::GetMemoryInformation()
        ///
        /// This call will throw a critical error on failure which will abort initialization of physical device NRI handle.
        void QueryMemoryInformation();

        /// @brief Queries surface capabilities and properties for this physical device and associated surface.
        /// Needs m_nativeHandle to be initialized before call
        void QuerySurfaceProperties();

        /// @brief Queries all distinct queue families available to this physical device.
        /// The result is stored in an array of NriPhysicalDeviceQueueFamilyInformation represented as 'm_queueFamilyInfos' field 
        /// and can be obtained by calling NriPhysicalDevice::GetQueueFamilyInfoArray()
        void QueryQueueFamilyProperties();

        VkPhysicalDevice m_nativeHandle = VK_NULL_HANDLE;
        VkSurfaceKHR m_surface = VK_NULL_HANDLE; // needed for querying surface capabilities of this physical device

        /// @brief An array of all supported and recognized device extensions by this physical device
        std::vector<VkExtensionProperties> m_supportedDeviceExtensionProperties;

        /// @brief A lookup table to quickly check whether an extension is supported by this physical device
        /// It stores extension names as string views from m_supportedDeviceExtensionProperties vector
        std::unordered_set<std::string_view> m_supportedDeviceExtensionLUT;

        NriPhysicalDeviceInformation m_info = NriPhysicalDeviceInformation();
        NriPhysicalDeviceMemoryInformation m_memoryInfo = NriPhysicalDeviceMemoryInformation();
        NriPhysicalDeviceSurfaceProperties m_surfaceProperties = NriPhysicalDeviceSurfaceProperties();
        std::vector<NriPhysicalDeviceQueueFamilyInformation> m_queueFamilyInfos;
    };

    /// @brief An enumeration of NRI-recognized surface types that can be created for presentation
    enum class ESurfaceType : uint16_t
    {
        /// For reference see VK_KHR_surface extension
        None,
        Win32,
    };

    /// @brief A simple wrapper around VkAppliationInfo
    struct NriInstanceInfo
    {
        /// Lifetime of appName underlying string must outlive NriInstance construction process
        std::string_view appName;
        uint32_t appVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);

        /// Lifetime of engineName underlying string must outlive NriInstance construction process
        std::string_view engineName;
        uint32_t engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);

        /// Surface type to be used for presentation. If no presentation is needed, ESurfaceType::None must be specified.
        ESurfaceType surfaceType = ESurfaceType::None;

        /// Native handle to a platform-specific window. This is only needed if surfaceType is not ESurfaceType::None.
        void* nativeWindowHandle = nullptr;
    };


    class NriInstance : public AtomicallyRefCounted<NriInstance>
    {
    public:
        NriInstance() = default;
        NriInstance(const NriInstanceInfo& instanceInfo);

        NriInstance(const NriInstance&) = delete;
        NriInstance& operator=(const NriInstance&) = delete;

        ~NriInstance();

        inline constexpr EApiVersion GetApiVersion() const noexcept { return m_apiVersion; }
        inline constexpr VkInstance GetNativeHandle() const noexcept { return m_nativeHandle; }
        inline constexpr VkSurfaceKHR GetSurface() const noexcept { return m_surface; }

        /// @brief Returns a span of all available physical devices that were enumerated during NRI instance creation
        /// If native window handle was provided during NRI instance creation, all physical devices would associate with the derived surface.
        /// To check whether a physical device supports presentation to the surface, use NriPhysicalDeviceQueueFamilyInformation::surfaceSupport field.
        inline constexpr std::span<const Arc<NriPhysicalDevice>> GetAvailablePhysicalDevices() const noexcept { return m_availablePhysicalDevices; }

    private:
        EApiVersion m_apiVersion = EApiVersion::Vk_1_0;
        VkInstance m_nativeHandle = VK_NULL_HANDLE;
        VkSurfaceKHR m_surface = VK_NULL_HANDLE;

        std::vector<Arc<NriPhysicalDevice>> m_availablePhysicalDevices;
    };

} // Warp::nri::vk namespace
