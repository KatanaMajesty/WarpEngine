#include "nri_vk_instance.h"

namespace Warp::nri::vk
{

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

        // An array of all required instance extensions to enable during NRI instance creation
        std::vector<const char*> enabledInstanceExtensions;
        // TODO: Might want to check whether surfaces are required and not add them in case headless rendering is intended
        if (instanceInfo.bSurfaceRequired)
        {
            enabledInstanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef VK_USE_PLATFORM_WIN32_KHR // Windows-specific extensions
            enabledInstanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif // Windows-specific extensions
        }
        enabledInstanceExtensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);

        auto createInfo = NRI_VK_STRUCT(VkInstanceCreateInfo);
        createInfo.flags = VkInstanceCreateFlagBits(0);
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = static_cast<uint32_t>(enabledInstanceLayers.size());
        createInfo.ppEnabledLayerNames = enabledInstanceLayers.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledInstanceExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledInstanceExtensions.data();

        NRI_VK_CHECK_RESULT(vkCreateInstance(&createInfo, nullptr, &m_nativeHandle), "Failed to create Vulkan instance!");
    }

    NriInstance::~NriInstance()
    {
        vkDestroyInstance(GetNativeHandle(), nullptr);
    }

    std::vector<Arc<NriPhysicalDevice>> NriInstance::QueryAvailablePhysicalDevices()
    {
        uint32_t physicalDeviceCount;
        NRI_VK_CHECK_RESULT(vkEnumeratePhysicalDevices(GetNativeHandle(), &physicalDeviceCount, nullptr), "Failed to enumerate physical devices");

        std::vector<VkPhysicalDevice> nativePhysicalDevices(physicalDeviceCount);
        NRI_VK_CHECK_RESULT(vkEnumeratePhysicalDevices(GetNativeHandle(), &physicalDeviceCount, nativePhysicalDevices.data()), "Failed to enumerate physical devices");

        std::vector<Arc<NriPhysicalDevice>> availablePhysicalDevices;
        availablePhysicalDevices.reserve(physicalDeviceCount);
        for (VkPhysicalDevice nativePhysicalDevice : nativePhysicalDevices)
        {
            availablePhysicalDevices.push_back(std::move(Arc<NriPhysicalDevice>::Make(nativePhysicalDevice)));
        }
        return availablePhysicalDevices;
    }

} // Warp::nri::vk