#include "nri_vk_instance.h"

#include "common/attr_defs.h"
#include "common/cross_log.h"

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
        enabledInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        // As per Vulkan specification of https://docs.vulkan.org/refpages/latest/refpages/source/VkInstanceCreateInfo.html:
        // To capture events that occur while creating or destroying an instance, an application can link a VkDebugReportCallbackCreateInfoEXT structure or a
        // VkDebugUtilsMessengerCreateInfoEXT structure to the pNext chain of the VkInstanceCreateInfo structure passed to vkCreateInstance.
        // This callback is only valid for the duration of the vkCreateInstance and the vkDestroyInstance call.
        auto validationMessengerCreateInfo = GetValidationLayerMessengerCreateInfo();

        auto createInfo = NRI_VK_STRUCT(VkInstanceCreateInfo);
        createInfo.pNext = &validationMessengerCreateInfo; // as per Vulkan spec
        createInfo.flags = VkInstanceCreateFlagBits(0);
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = static_cast<uint32_t>(enabledInstanceLayers.size());
        createInfo.ppEnabledLayerNames = enabledInstanceLayers.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledInstanceExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledInstanceExtensions.data();

        NRI_VK_CHECK_RESULT(vkCreateInstance(&createInfo, nullptr, &m_nativeHandle), "Failed to create Vulkan instance!");

        // after creating instance, initialize its validaiton messanger
        InitValidationLayerMessenger();
    }

    NriInstance::~NriInstance()
    {
        if (m_validationLayerMessenger != VK_NULL_HANDLE)
        {
            auto destroyDebugUtilsMessengerEXT = NRI_VK_INSTANCE_PROC_ADDR(vkDestroyDebugUtilsMessengerEXT, GetNativeHandle());
            WARP_ASSERT(destroyDebugUtilsMessengerEXT != nullptr, "Failed to obtain vkDestroyDebugUtilsMessengerEXT");
            destroyDebugUtilsMessengerEXT(GetNativeHandle(), m_validationLayerMessenger, nullptr);
        }
        vkDestroyInstance(GetNativeHandle(), nullptr);
    }

    std::vector<Arc<NriPhysicalDevice>> NriInstance::QueryAvailablePhysicalDevices()
    {
        uint32_t physicalDeviceCount;
        NRI_VK_CHECK_RESULT(vkEnumeratePhysicalDevices(GetNativeHandle(), &physicalDeviceCount, nullptr), "Failed to enumerate physical devices");

        std::vector<VkPhysicalDevice> nativePhysicalDevices(physicalDeviceCount);
        NRI_VK_CHECK_RESULT(vkEnumeratePhysicalDevices(GetNativeHandle(), &physicalDeviceCount, nativePhysicalDevices.data()),
                            "Failed to enumerate physical devices");

        std::vector<Arc<NriPhysicalDevice>> availablePhysicalDevices;
        availablePhysicalDevices.reserve(physicalDeviceCount);
        for (VkPhysicalDevice nativePhysicalDevice : nativePhysicalDevices)
        {
            availablePhysicalDevices.push_back(std::move(Arc<NriPhysicalDevice>::Make(nativePhysicalDevice)));
        }
        return availablePhysicalDevices;
    }

    static VkBool32 ValidationLayerMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                     VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                     const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                     void* pUserData)
    {
        static constexpr auto GetMessageSeverity = [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity) -> std::string_view
        {
            switch (messageSeverity)
            {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return "VERBOSE";
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: return "INFO";
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return "WARNING";
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: return "ERROR";
            default: return "UNKNOWN";
            }
        };
        static constexpr auto GetLatestDebugLabelName = [](uint32_t labelCount, const VkDebugUtilsLabelEXT* labels) -> std::string_view
        {
            // According to Vulkan spec from https://docs.vulkan.org/refpages/latest/refpages/source/VkDebugUtilsMessengerCallbackDataEXT.html:
            // Since adding queue and command buffer labels behaves like pushing and popping onto a stack, the order of both pQueueLabels and pCmdBufLabels is
            // based on the order the labels were defined. The result is that the first label in either pQueueLabels or pCmdBufLabels will be the first defined
            // (and therefore the oldest) while the last label in each list will be the most recent.
            if (labelCount == 0)
            {
                return "NONE";
            }
            return labels[labelCount - 1].pLabelName;
        };
        std::string message;
        if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
        {
            message = std::format("[NRI Vulkan General] {}: {}", pCallbackData->pMessageIdName, pCallbackData->pMessage);
        }
        else
        {
            // Validation and performance messages are more important, so we log them with more details
            message = std::format("[NRI Vulkan Validation] {}"
                                  "\n\t{}: {}",
                                  string_VkDebugUtilsMessageTypeFlagsEXT(messageTypes), // Vulkan message types
                                  pCallbackData->pMessageIdName,                        // actual message body
                                  pCallbackData->pMessage);
            if (pCallbackData->queueLabelCount > 0 || pCallbackData->cmdBufLabelCount > 0)
            {
                message.append(std::format("\n\tQueueLabel: {}, CmdBufLabel: {}",
                                           GetLatestDebugLabelName(pCallbackData->queueLabelCount, pCallbackData->pQueueLabels),  // latest queue label
                                           GetLatestDebugLabelName(pCallbackData->cmdBufLabelCount, pCallbackData->pCmdBufLabels) // latest command buffer label
                                           ));
            }
        }
        switch (messageSeverity)
        {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: WARP_A_FALLTHROUGH;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: WARP_LOG_INFO(ELoggerType::NriLogger, "{}", message); break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: WARP_LOG_WARN(ELoggerType::NriLogger, "{}", message); break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: WARP_LOG_ERROR(ELoggerType::NriLogger, "{}", message); break;
        default:
            WARP_LOG_ERROR(ELoggerType::NriLogger, "Unknown message severity: {}. {}", string_VkDebugUtilsMessageSeverityFlagBitsEXT(messageSeverity), message);
            break;
        }
        // The application should always return VK_FALSE. The VK_TRUE value is reserved for use in layer development.
        return VK_FALSE;
    }

    VkDebugUtilsMessengerCreateInfoEXT NriInstance::GetValidationLayerMessengerCreateInfo()
    {
        auto createInfo = NRI_VK_STRUCT(VkDebugUtilsMessengerCreateInfoEXT);
        createInfo.flags = 0; // reserved
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = ValidationLayerMessengerCallback;
        createInfo.pUserData = nullptr; // not used for now
        return createInfo;
    }

    void NriInstance::InitValidationLayerMessenger()
    {
        // TODO: For now NriInstance always tries to enable validation layers,
        // so we just assume they are present and we always init validation layer messenger. In the future we might want to only
        // init validation layer messenger in case validation layers are actually enabled
        auto createInfo = GetValidationLayerMessengerCreateInfo();
        auto createDebugUtilsMessengerEXT = NRI_VK_INSTANCE_PROC_ADDR(vkCreateDebugUtilsMessengerEXT, GetNativeHandle());
        WARP_ASSERT(createDebugUtilsMessengerEXT != nullptr, "Failed to obtain vkCreateDebugUtilsMessengerEXT");
        NRI_VK_CHECK_RESULT(createDebugUtilsMessengerEXT(GetNativeHandle(), &createInfo, nullptr, &m_validationLayerMessenger),
                            "Failed to create validation layer messenger");
    }

} // Warp::nri::vk