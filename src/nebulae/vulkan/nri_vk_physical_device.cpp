#include "nri_vk_physical_device.h"

#include <ranges>

namespace Warp::nri::vk
{

    NriPhysicalDevice::NriPhysicalDevice(VkPhysicalDevice nativeHandle)
        : m_nativeHandle(nativeHandle)
    {
        WARP_ASSERT(nativeHandle != VK_NULL_HANDLE, "Null handle was provided for NriPhysicalDevice");

        // Query all necessary physical device information
        QuerySupportedExtensions();
        QueryInformation();
        QueryMemoryInformation();
        QueryQueueFamilyProperties();

        uint32_t extensionPropertyCount;
        NRI_VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(GetNativeHandle(), nullptr, &extensionPropertyCount, nullptr),
            "Failed to enumerate device extensions");
    }

    NriPhysicalDeviceSurfaceProperties NriPhysicalDevice::QuerySurfaceProperties(VkSurfaceKHR nativeSurface)
    {
        WARP_ASSERT(nativeSurface != VK_NULL_HANDLE, "Invalid native surface handle provided to query physical device surface properties");

        VkPhysicalDevice nativeHandle = GetNativeHandle();
        NriPhysicalDeviceSurfaceProperties surfaceProperties = NriPhysicalDeviceSurfaceProperties();

        // query surface capabilities
        {
            // TODO: VK_EXT_full_screen_exclusive && VK_KHR_win32_surface
            // auto fullScreenExclusiveInfo = NRI_VK_STRUCT(VkSurfaceFullScreenExclusiveInfoEXT);

            auto surfaceInfo = NRI_VK_STRUCT(VkPhysicalDeviceSurfaceInfo2KHR);
            surfaceInfo.surface = nativeSurface;

            auto surfaceCapabilities2KHR = NRI_VK_STRUCT(VkSurfaceCapabilities2KHR);
            NRI_VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilities2KHR(nativeHandle, &surfaceInfo, &surfaceCapabilities2KHR),
                "Failed to query physical device surface capabilities");

            const VkSurfaceCapabilitiesKHR& surfaceCapabilities = surfaceCapabilities2KHR.surfaceCapabilities;
            surfaceProperties.minImageCount = surfaceCapabilities.minImageCount;
            surfaceProperties.maxImageCount = surfaceCapabilities.maxImageCount;
            surfaceProperties.minExtent = surfaceCapabilities.minImageExtent;
            surfaceProperties.maxExtent = surfaceCapabilities.maxImageExtent;
            surfaceProperties.currentExtent = surfaceCapabilities.currentExtent;
            surfaceProperties.supportedUsageFlags = surfaceCapabilities.supportedUsageFlags;
        }

        // Query surface formats available on this physical device
        {
            uint32_t numSurfaceFormats = 0;
            NRI_VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(nativeHandle, nativeSurface, &numSurfaceFormats, nullptr),
                "Failed to query physical device surface formats");

            surfaceProperties.availableSurfaceFormats.resize(numSurfaceFormats);
            NRI_VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(nativeHandle, nativeSurface, &numSurfaceFormats, surfaceProperties.availableSurfaceFormats.data()),
                "Failed to query physical device surface formats");
        }

        // Query available present modes
        {
            uint32_t numPresentModes = 0;
            NRI_VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(nativeHandle, nativeSurface, &numPresentModes, nullptr),
                "Failed to query physical device surface present modes");

            surfaceProperties.availablePresentModes.resize(numPresentModes);
            NRI_VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(nativeHandle, nativeSurface, &numPresentModes, surfaceProperties.availablePresentModes.data()),
                "Failed to query physical device surface present modes");
        }
        return surfaceProperties;
    }

    bool NriPhysicalDevice::IsSurfaceSupportedByQueueFamily(VkSurfaceKHR nativeSurface, const NriPhysicalDeviceQueueFamilyInformation& queueFamilyInfo) const
    {
        // Check whether this queue family supports presentation to the surface provided
        VkBool32 isSurfaceSupported = VK_FALSE;
        NRI_VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(GetNativeHandle(), queueFamilyInfo.queueFamilyIndex, nativeSurface, &isSurfaceSupported),
            "Failed to get physical device support for Vulkan surface");
        return (isSurfaceSupported == VK_TRUE);
    }

    void NriPhysicalDevice::QuerySupportedExtensions()
    {
        uint32_t extensionPropertyCount;
        NRI_VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(GetNativeHandle(), nullptr, &extensionPropertyCount, nullptr),
            "Failed to enumerate device extensions");

        m_supportedDeviceExtensionProperties = std::vector<VkExtensionProperties>(extensionPropertyCount);
        NRI_VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(GetNativeHandle(), nullptr, &extensionPropertyCount, m_supportedDeviceExtensionProperties.data()),
            "Failed to enumerate device extensions");

        // transform plain unordered access array into a look-up hash-set to easily query for all recognized extensions an engine might require
        m_supportedDeviceExtensionLUT.clear();
        m_supportedDeviceExtensionLUT.insert_range(
            m_supportedDeviceExtensionProperties | std::views::transform(
                                                       [](const VkExtensionProperties& extProperties)
                                                       {
                                                           return std::string_view(extProperties.extensionName);
                                                       }));
    }

    void NriPhysicalDevice::QueryInformation()
    {
        // First query physical device information to populate NriDeviceInformation struct
        auto physicalDeviceDriverProperties = NRI_VK_STRUCT(VkPhysicalDeviceDriverProperties);
        auto physicalDeviceProperties = NRI_VK_STRUCT(VkPhysicalDeviceProperties2, &physicalDeviceDriverProperties);
        vkGetPhysicalDeviceProperties2(GetNativeHandle(), &physicalDeviceProperties);

        m_info = NriPhysicalDeviceInformation();
        m_info.deviceName = physicalDeviceProperties.properties.deviceName;
        m_info.driverName = physicalDeviceDriverProperties.driverName;
        m_info.driverInfo = physicalDeviceDriverProperties.driverInfo;
        m_info.vendor = static_cast<EPhysicalDeviceVendor>(physicalDeviceProperties.properties.vendorID & 0xffff);
        m_info.type = physicalDeviceProperties.properties.deviceType;
        m_info.supportedApiVersion = static_cast<EApiVersion>(physicalDeviceProperties.properties.apiVersion);
    }

    void NriPhysicalDevice::QueryMemoryInformation()
    {
        auto physicalDeviceMemoryProperties = NRI_VK_STRUCT(VkPhysicalDeviceMemoryProperties2);
        vkGetPhysicalDeviceMemoryProperties2(GetNativeHandle(), &physicalDeviceMemoryProperties);

        VkPhysicalDeviceMemoryProperties& baseMemProperties = physicalDeviceMemoryProperties.memoryProperties;

        m_memoryInfo = NriPhysicalDeviceMemoryInformation();
        m_memoryInfo.totalSizeInBytes = 0;
        m_memoryInfo.memoryHeaps.resize(baseMemProperties.memoryHeapCount);

        // to determine total size of memory (in bytes) available to this physical device
        // we iterate over each available heap and sum it up essentially
        for (uint32_t heapIndex = 0; heapIndex < baseMemProperties.memoryHeapCount; ++heapIndex)
        {
            const VkMemoryHeap& srcHeap = baseMemProperties.memoryHeaps[heapIndex];

            m_memoryInfo.memoryHeaps.at(heapIndex).heapTotalSizeInBytes = srcHeap.size;
            m_memoryInfo.totalSizeInBytes += srcHeap.size;
        }

        // a separate loop here in order to determine memory properties of each heap
        for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < baseMemProperties.memoryTypeCount; ++memoryTypeIndex)
        {
            const VkMemoryType& srcMemoryType = baseMemProperties.memoryTypes[memoryTypeIndex];

            NriPhysicalDeviceMemoryHeap& memoryHeap = m_memoryInfo.memoryHeaps[srcMemoryType.heapIndex];
            memoryHeap.memoryProperties.Set(EMemoryProperty::DeviceLocal, srcMemoryType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            memoryHeap.memoryProperties.Set(EMemoryProperty::HostVisible, srcMemoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            memoryHeap.memoryProperties.Set(EMemoryProperty::HostCoherent, srcMemoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            memoryHeap.memoryProperties.Set(EMemoryProperty::HostCached, srcMemoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
            memoryHeap.memoryProperties.Set(EMemoryProperty::LazilyAllocated, srcMemoryType.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);
        }
    }

    void NriPhysicalDevice::QueryQueueFamilyProperties()
    {
        VkPhysicalDevice nativeHandle = GetNativeHandle();

        // Starting from Vulkan 1.4 extension VK_KHR_global_priority (or VK_EXT_global_priority_query) is ratified in core API
        // Check whether global priority API is available on this device
        bool hasGlobalPriorityProperties = GetApiVersion() >= EApiVersion::Vk_1_4 || IsExtensionSupported(VK_KHR_GLOBAL_PRIORITY_EXTENSION_NAME);

        uint32_t numProperties = 0;
        vkGetPhysicalDeviceQueueFamilyProperties2(nativeHandle, &numProperties, nullptr);

        auto physicalDeviceQueueFamilyProperties = std::vector(numProperties, NRI_VK_STRUCT(VkQueueFamilyProperties2));
        vkGetPhysicalDeviceQueueFamilyProperties2(nativeHandle, &numProperties, physicalDeviceQueueFamilyProperties.data());

        m_queueFamilyInfos = std::vector<NriPhysicalDeviceQueueFamilyInformation>(numProperties);
        for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < numProperties; ++queueFamilyIndex)
        {
            NriPhysicalDeviceQueueFamilyInformation& queueFamilyInfo = m_queueFamilyInfos.at(queueFamilyIndex);
            const VkQueueFamilyProperties& srcFamilyProperties = physicalDeviceQueueFamilyProperties.at(queueFamilyIndex).queueFamilyProperties;

            queueFamilyInfo.queueFamilyIndex = queueFamilyIndex;
            queueFamilyInfo.queueCount = srcFamilyProperties.queueCount;
            queueFamilyInfo.capabilities.Set(EQueueCapability::Graphics, srcFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT);
            queueFamilyInfo.capabilities.Set(EQueueCapability::Compute, srcFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT);
            queueFamilyInfo.capabilities.Set(EQueueCapability::Transfer, srcFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT);
        }
    }

} // Warp::nri::vk namespace