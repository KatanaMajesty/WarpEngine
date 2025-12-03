#include "nri_vk_instance.h"

#include <ranges>

namespace Warp::nri::vk
{

    NriPhysicalDevice::NriPhysicalDevice(VkPhysicalDevice nativeHandle, VkSurfaceKHR surface)
        : m_nativeHandle(nativeHandle)
        , m_surface(surface)
    {
        WARP_ASSERT(nativeHandle != VK_NULL_HANDLE, "Null handle was provided for NriPhysicalDevice");

        // Query all necessary physical device information
        QuerySupportedExtensions();
        QueryInformation();
        QueryMemoryInformation();
        QuerySurfaceProperties();
        QueryQueueFamilyProperties();

        uint32_t extensionPropertyCount;
        NRI_VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(GetNativeHandle(), nullptr, &extensionPropertyCount, nullptr),
            "Failed to enumerate device extensions");
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
        auto physicalDeviceProperties       = NRI_VK_STRUCT(VkPhysicalDeviceProperties2, &physicalDeviceDriverProperties);
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

    void NriPhysicalDevice::QuerySurfaceProperties()
    {
        VkPhysicalDevice nativeHandle = GetNativeHandle();
        VkSurfaceKHR surface = GetSurface();
        NriPhysicalDeviceSurfaceProperties& surfaceProperties = m_surfaceProperties;

        auto surfaceInfo = NRI_VK_STRUCT(VkPhysicalDeviceSurfaceInfo2KHR);
        surfaceInfo.surface = surface;

        // query surface capabilities
        {
            // TODO: VK_EXT_full_screen_exclusive && VK_KHR_win32_surface
            // auto fullScreenExclusiveInfo = NRI_VK_STRUCT(VkSurfaceFullScreenExclusiveInfoEXT);

            auto surfaceCapabilities2KHR = NRI_VK_STRUCT(VkSurfaceCapabilities2KHR);
            NRI_VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilities2KHR(nativeHandle, &surfaceInfo, &surfaceCapabilities2KHR),
                "Failed to query physical device surface capabilities");

            const VkSurfaceCapabilitiesKHR& surfaceCapabilities = surfaceCapabilities2KHR.surfaceCapabilities;
            surfaceProperties.minImageCount = surfaceCapabilities.minImageCount;
            surfaceProperties.maxImageCount = surfaceCapabilities.maxImageCount;
            surfaceProperties.minExtent = surfaceCapabilities.minImageExtent;
            surfaceProperties.maxExtent = surfaceCapabilities.maxImageExtent;
            surfaceProperties.supportedUsageFlags = surfaceCapabilities.supportedUsageFlags;
        }

        // Query surface formats available on this physical device
        {
            uint32_t numSurfaceFormats = 0;
            NRI_VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormats2KHR(nativeHandle, &surfaceInfo, &numSurfaceFormats, nullptr),
                "Failed to query physical device surface formats");

            surfaceProperties.availableSurfaceFormats.resize(numSurfaceFormats);
            NRI_VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormats2KHR(nativeHandle, &surfaceInfo, &numSurfaceFormats, surfaceProperties.availableSurfaceFormats.data()),
                "Failed to query physical device surface formats");
        }

        // Query available present modes
        {
            uint32_t numPresentModes = 0;
            NRI_VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(nativeHandle, surface, &numPresentModes, nullptr),
                "Failed to query physical device surface present modes");

            surfaceProperties.availablePresentModes.resize(numPresentModes);
            NRI_VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(nativeHandle, surface, &numPresentModes, surfaceProperties.availablePresentModes.data()),
                "Failed to query physical device surface present modes");
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

            // Check whether this queue family supports presentation to the surface specified during physical device NRI handle creation
            VkBool32 isSurfaceSupported = VK_FALSE;
            NRI_VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(nativeHandle, queueFamilyIndex, GetSurface(), &isSurfaceSupported));
            queueFamilyInfo.surfaceSupport = (isSurfaceSupported == VK_TRUE);
        }
    }

    NriInstance::NriInstance(const NriInstanceInfo& instanceInfo)
    {
        /// Because Vulkan 1.0 implementations may fail with VK_ERROR_INCOMPATIBLE_DRIVER, applications should determine the version
        /// of Vulkan available before calling vkCreateInstance.
        /// If the vkGetInstanceProcAddr returns NULL for vkEnumerateInstanceVersion, it is a Vulkan 1.0 implementation.
        /// Otherwise, the application can call vkEnumerateInstanceVersion to determine the version of Vulkan.
        m_apiVersion = EApiVersion::Vk_1_0;
        auto fnEnumerateInstanceVersion = NRI_VK_INSTANCE_PROC_ADDR(vkEnumerateInstanceVersion, nullptr);
        if (fnEnumerateInstanceVersion)
        {
            uint32_t apiVersionResult;
            NRI_VK_CHECK_RESULT(fnEnumerateInstanceVersion(&apiVersionResult), "Failed to query max instance version");
            m_apiVersion = static_cast<EApiVersion>(apiVersionResult);
        }

        // We store application info inside of instance
        auto appInfo = NRI_VK_STRUCT(VkApplicationInfo);
        appInfo.pApplicationName = instanceInfo.appName.data();
        appInfo.applicationVersion = instanceInfo.appVersion;
        appInfo.pEngineName = instanceInfo.engineName.data();
        appInfo.engineVersion = instanceInfo.engineVersion;
        appInfo.apiVersion = EnumValue(m_apiVersion);

        // An array of all required instance layers to enable during NRI instance creation
        std::vector<const char*> enabledInstanceLayers;
        // TODO: for now always enable KHONOS_validation
        enabledInstanceLayers.push_back("VK_LAYER_KHRONOS_validation");

        bool isSurfaceRequired = (instanceInfo.nativeWindowHandle != nullptr);
        bool isWin32SurfaceRequired = instanceInfo.surfaceType == ESurfaceType::Win32;
        WARP_ASSERT(isSurfaceRequired || instanceInfo.surfaceType == ESurfaceType::None,
            "Vulkan NRI instance requires a surface type for presentation, but ESurfaceType::None was specified!");

        // An array of all required instance extensions to enable during NRI instance creation
        std::vector<const char*> enabledInstanceExtensions;
        // TODO: Might want to check whether surfaces are required and not add them in case headless rendering is intended
        if (isSurfaceRequired)
            enabledInstanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

        if (isWin32SurfaceRequired)
            enabledInstanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

        enabledInstanceExtensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);

        auto createInfo = NRI_VK_STRUCT(VkInstanceCreateInfo);
        createInfo.flags = VkInstanceCreateFlagBits(0);
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = static_cast<uint32_t>(enabledInstanceLayers.size());
        createInfo.ppEnabledLayerNames = enabledInstanceLayers.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledInstanceExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledInstanceExtensions.data();

        NRI_VK_CHECK_RESULT(vkCreateInstance(&createInfo, nullptr, &m_nativeHandle), "Failed to create Vulkan instance!");

        uint32_t physicalDeviceCount;
        NRI_VK_CHECK_RESULT(vkEnumeratePhysicalDevices(GetNativeHandle(), &physicalDeviceCount, nullptr), "Failed to enumerate physical devices");

        std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
        NRI_VK_CHECK_RESULT(vkEnumeratePhysicalDevices(GetNativeHandle(), &physicalDeviceCount, physicalDevices.data()), "Failed to enumerate physical devices");

        // Check if surface support is required
        // For now we just assume that surface is needed, because Warp Engine won't currently support headless rendering
        WARP_ASSERT(instanceInfo.surfaceType != ESurfaceType::None,
            "Vulkan NRI instance requires a surface type for presentation, but ESurfaceType::None was specified!");
        WARP_ASSERT(instanceInfo.nativeWindowHandle != nullptr,
            "Vulkan NRI instance requires a valid native window handle for presentation, but nullptr was specified!");

        // Create platform-specific surface for presentation
        // TODO: Currently only Win32 surface is supported
        if (instanceInfo.surfaceType == ESurfaceType::Win32)
        {
            auto surfaceCreateInfo = NRI_VK_STRUCT(VkWin32SurfaceCreateInfoKHR);
            surfaceCreateInfo.flags = 0; // flags must be 0
            surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
            surfaceCreateInfo.hwnd = static_cast<HWND>(instanceInfo.nativeWindowHandle);
            NRI_VK_CHECK_RESULT(vkCreateWin32SurfaceKHR(GetNativeHandle(), &surfaceCreateInfo, nullptr, &m_surface), "Failed to create Vulkan win32 surface");
        }
        WARP_ASSERT(m_surface != VK_NULL_HANDLE, "Vulkan surface was not created properly!");

        m_availablePhysicalDevices.reserve(physicalDeviceCount);
        for (VkPhysicalDevice physicalDevice : physicalDevices)
        {
            m_availablePhysicalDevices.push_back(std::move(Arc<NriPhysicalDevice>::Make(physicalDevice, GetSurface())));
        }
    }

    NriInstance::~NriInstance()
    {
        vkDestroySurfaceKHR(GetNativeHandle(), GetSurface(), nullptr);
        vkDestroyInstance(GetNativeHandle(), nullptr);
    }

} // Warp::nri::vk