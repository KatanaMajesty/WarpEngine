#include "nri_vk_device.h"

#include <array>
#include <ranges>
#include <queue>
#include <functional>

namespace Warp::nri::vk
{

    NriDevice::NriDevice(const NriDeviceInfo& deviceInfo)
        : m_instance(deviceInfo.instance)
    {
        WARP_ASSERT(m_instance->GetNativeHandle() != VK_NULL_HANDLE, "NRI instance was not initialized correctly");

        // wrap extension names with std::string_view as all NRI API functions use string_view for extension names
        std::vector<const char*> requiredDeviceExtensions;

        if (deviceInfo.bSwapchainRequired)
        {
            // if swapchain is required we need to enable swapchain extension
            requiredDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        }

        requiredDeviceExtensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        requiredDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        requiredDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME); // required by KHR_acceleration_structure
        requiredDeviceExtensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        requiredDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

        std::vector<NriSuitablePhysicalDeviceInfo> suitableDevices =
            QueryAllSuitableDevices(SuitableDeviceQueryInfo{ .surface = deviceInfo.surface, .requiredDeviceExtensions = requiredDeviceExtensions });
        // Select the best suitable device
        {
            NriSuitablePhysicalDeviceInfo selectedPhysicalDeviceInfo = SelectBestSuitableDevice(suitableDevices);
            // explicitly store the physical device used to create this logical device
            m_physicalDevice = selectedPhysicalDeviceInfo.physicalDevice;
            // also store queue family index information for every queue type, as it might be required at runtime for command buffer recording, synchronization, etc.
            // one instance of such, is VK_SHARING_MODE_CONCURRENT during buffer and image creation, 
            // where we need to provide queue family indices of all queues that will be sharing a particular resource
            m_deviceQueueFamilyInfos = selectedPhysicalDeviceInfo.queueFamilyInfos;

            WARP_LOG_INFO(ELoggerType::NriLogger, "Physical device '{}' selected for NRI device creation", m_physicalDevice->GetInformation().deviceName);
        }

        std::array<VkDeviceQueueCreateInfo, EnumValue(EDeviceQueueType::NumTypes)> queueCreateInfoArray;
        for (uint32_t deviceQueueIndex = 0; deviceQueueIndex < queueCreateInfoArray.size(); ++deviceQueueIndex)
        {
            VkDeviceQueueCreateInfo& queueCreateInfo = queueCreateInfoArray.at(deviceQueueIndex);

            // Initialize structures sType and pNext, with fields.
            // We set device-local priority to 1.0f as we instead prefer to use global-scope priorities provided by VkDeviceQueueGlobalPriorityCreateInfo
            float queuePriority = 1.0f;
            queueCreateInfo = NRI_VK_STRUCT(VkDeviceQueueCreateInfo);
            queueCreateInfo.queueFamilyIndex = m_deviceQueueFamilyInfos.at(deviceQueueIndex).queueFamilyIndex;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
        }

        // VK_EXT_mesh_shader -> Specify features required for mesh shaders
        WARP_ASSERT(m_physicalDevice->IsExtensionSupported(VK_EXT_MESH_SHADER_EXTENSION_NAME));
        auto meshShaderFeatures = NRI_VK_STRUCT(VkPhysicalDeviceMeshShaderFeaturesEXT);
        meshShaderFeatures.meshShader = VK_TRUE;
        meshShaderFeatures.taskShader = VK_TRUE;

        // VK_KHR_acceleration_structure -> Specify features required for acceleration structures
        // Set pNext to meshShaderFeatures to chain the structures together
        WARP_ASSERT(m_physicalDevice->IsExtensionSupported(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME));
        auto accelerationStructureFeatures = NRI_VK_STRUCT(VkPhysicalDeviceAccelerationStructureFeaturesKHR, &meshShaderFeatures);
        accelerationStructureFeatures.accelerationStructure = VK_TRUE;

        WARP_ASSERT(m_physicalDevice->IsExtensionSupported(VK_KHR_RAY_QUERY_EXTENSION_NAME));
        // VK_KHR_ray_query -> Specify features required for ray queries
        auto rayQueryFeatures = NRI_VK_STRUCT(VkPhysicalDeviceRayQueryFeaturesKHR, &accelerationStructureFeatures);
        rayQueryFeatures.rayQuery = VK_TRUE;

        // VK_KHR_ray_tracing_pipeline -> Specify features required for ray tracing pipeline
        WARP_ASSERT(m_physicalDevice->IsExtensionSupported(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME));
        auto rayTracingPipelineFeatures = NRI_VK_STRUCT(VkPhysicalDeviceRayTracingPipelineFeaturesKHR, &rayQueryFeatures);
        rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
        rayTracingPipelineFeatures.rayTracingPipelineTraceRaysIndirect = VK_TRUE;

        // As of Vulkan 1.2 it is recommended to provide VkPhysicalDeviceFeatures2 into pNext chain of VkDeviceCreateInfo if specific features are required.
        auto physicalDeviceFeatures2 = NRI_VK_STRUCT(VkPhysicalDeviceFeatures2, &rayTracingPipelineFeatures);

        auto deviceCreateInfo = NRI_VK_STRUCT(VkDeviceCreateInfo);
        deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfoArray.size());
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfoArray.data();
        // deprecated and should not be used. Device-only layers are now marked as legacy, and Vulkan no longer distinguishes between instance and device layers
        // https://registry.khronos.org/VulkanSC/specs/1.0-extensions/html/vkspec.html#legacy-devicelayers
        deviceCreateInfo.enabledLayerCount = 0;
        deviceCreateInfo.ppEnabledLayerNames = nullptr;

        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
        // This field is legacy. See https://registry.khronos.org/vulkansc/specs/1.0-extensions/html/vkspec.html#legacy-gpdp2.
        // Instead provide VkPhysicalDeviceFeatures2 into pNext chain of VkDeviceCreateInfo if specific features are required.
        deviceCreateInfo.pEnabledFeatures = nullptr;
        NRI_VK_CHECK_RESULT(vkCreateDevice(m_physicalDevice->GetNativeHandle(), &deviceCreateInfo, nullptr, &m_nativeHandle),
                            "Failed to create Vulkan logical device");

        // Obtain Vulkan queue handles for each queue we've just requested
        for (uint32_t queueTypeIndex = 0; queueTypeIndex < m_deviceQueues.size(); ++queueTypeIndex)
        {
            const NriPhysicalDeviceQueueFamilyInformation& queueFamilyInfo = m_deviceQueueFamilyInfos.at(queueTypeIndex);
            vkGetDeviceQueue(GetNativeHandle(), queueFamilyInfo.queueFamilyIndex, 0, &m_deviceQueues[queueTypeIndex]);
        }
        // Create command pools for every queue family this device is suitable for
        for (uint32_t commandPoolIndex = 0; commandPoolIndex < m_commandPools.size(); ++commandPoolIndex)
        {
            const NriPhysicalDeviceQueueFamilyInformation& queueFamilyInfo = m_deviceQueueFamilyInfos.at(commandPoolIndex);

            auto commandPoolInfo = NRI_VK_STRUCT(VkCommandPoolCreateInfo);
            // Allow command buffers to be rerecorded individually, without this flag they all have to be reset together
            commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            commandPoolInfo.queueFamilyIndex = queueFamilyInfo.queueFamilyIndex;
            // TODO: Check if multiple command pools are fine for the same queue family index.
            // In Warp NRI if physical device doesn't have transfer/compute queues supported universal queue index will be assigned.
            // Because of that this code snippet potentially could invoke vkCreateCommandPool multiple times for the same queue family index
            NRI_VK_CHECK_RESULT(vkCreateCommandPool(GetNativeHandle(), &commandPoolInfo, nullptr, &m_commandPools.at(commandPoolIndex)),
                                "Failed to create command pool");
        }
        // Create Vulkan Memory Allocator for this device (VMA)
        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.physicalDevice = m_physicalDevice->GetNativeHandle();
        allocatorCreateInfo.device = GetNativeHandle();
        allocatorCreateInfo.preferredLargeHeapBlockSize = 256ull * 1024ull * 1024ull; // 256 MiB, default value recommended by VMA documentation for large heaps (>1 GiB)
        allocatorCreateInfo.pHeapSizeLimit = nullptr; // optional, we don't want to set custom heap size limits, so set to nullptr to use defaults (which is no limit)
        allocatorCreateInfo.instance = m_instance->GetNativeHandle();
        allocatorCreateInfo.vulkanApiVersion = EnumValue(m_instance->GetApiVersion());
#if defined(VMA_EXTERNAL_MEMORY) && VMA_EXTERNAL_MEMORY == 1
        allocatorCreateInfo.pTypeExternalMemoryHandleTypes = nullptr;
#endif // defined(VMA_EXTERNAL_MEMORY) && VMA_EXTERNAL_MEMORY == 1
        NRI_VK_CHECK_RESULT(vmaCreateAllocator(&allocatorCreateInfo, &m_memoryAllocator), "Failed to create Vulkan Memory Allocator");
    }

    NriDevice::~NriDevice()
    {
        vmaDestroyAllocator(m_memoryAllocator);
        for (uint32_t commandPoolIndex = 0; commandPoolIndex < m_commandPools.size(); ++commandPoolIndex)
        {
            vkDestroyCommandPool(GetNativeHandle(), m_commandPools.at(commandPoolIndex), nullptr);
        }
        vkDestroyDevice(GetNativeHandle(), nullptr);
    }

    void NriDevice::BeginDebugLabel(VkCommandBuffer commandBuffer, std::string_view labelName, glm::vec4 labelColor) const noexcept 
    {
        auto labelInfo = NRI_VK_STRUCT(VkDebugUtilsLabelEXT);
        labelInfo.pLabelName = labelName.data();
        labelInfo.color[0] = labelColor.r;
        labelInfo.color[1] = labelColor.g;
        labelInfo.color[2] = labelColor.b;
        labelInfo.color[3] = labelColor.a;
        static PFN_vkCmdBeginDebugUtilsLabelEXT cmdBeginDebugUtilsLabelEXT = nullptr;
        if (!cmdBeginDebugUtilsLabelEXT)
        {
            cmdBeginDebugUtilsLabelEXT = NRI_VK_INSTANCE_PROC_ADDR(vkCmdBeginDebugUtilsLabelEXT, m_instance->GetNativeHandle());
            WARP_ASSERT(cmdBeginDebugUtilsLabelEXT, "Failed to load vkCmdBeginDebugUtilsLabelEXT function pointer");
        }
        cmdBeginDebugUtilsLabelEXT(commandBuffer, &labelInfo);
    }

    void NriDevice::EndDebugLabel(VkCommandBuffer commandBuffer) const noexcept 
    {
        static PFN_vkCmdEndDebugUtilsLabelEXT cmdEndDebugUtilsLabelEXT = nullptr;
        if (!cmdEndDebugUtilsLabelEXT)
        {
            cmdEndDebugUtilsLabelEXT = NRI_VK_INSTANCE_PROC_ADDR(vkCmdEndDebugUtilsLabelEXT, m_instance->GetNativeHandle());
            WARP_ASSERT(cmdEndDebugUtilsLabelEXT, "Failed to load vkCmdEndDebugUtilsLabelEXT function pointer");
        }
        cmdEndDebugUtilsLabelEXT(commandBuffer);
    }

    std::vector<NriSuitablePhysicalDeviceInfo> NriDevice::QueryAllSuitableDevices(const SuitableDeviceQueryInfo& queryInfo)
    {
        std::vector<NriSuitablePhysicalDeviceInfo> suitablePhysicalDeviceInfoArray;

        // This iteration will determine what physical devices can be used for this virtual device creation
        for (Arc<NriPhysicalDevice> physicalDevice : m_instance->QueryAvailablePhysicalDevices())
        {
            // first of all we want to gather all devices that support required extensions
            for (std::string_view requiredExtension : queryInfo.requiredDeviceExtensions)
            {
                if (!physicalDevice->IsExtensionSupported(requiredExtension))
                {
                    WARP_LOG_WARN(ELoggerType::NriLogger,
                                  "Physical device '{}' does not support required extension '{}'. Skipping...",
                                  physicalDevice->GetInformation().deviceName,
                                  requiredExtension);
                    continue;
                }
            }

            // Check for minimal surface capabilities support that would be used for swapchain creation.
            // Minimal surface capabilities include:
            //  maxImages > 1
            //  availableSurfaceFormats > 0
            //  availablePresentModes > 0
            if (queryInfo.surface && queryInfo.surface->GetNativeHandle() != VK_NULL_HANDLE)
            {
                // If valid NriSurface was provided during NriDevice creation we need to check whether it has minimal valid support for this surface handle
                NriPhysicalDeviceSurfaceProperties surfaceProperties = physicalDevice->QuerySurfaceProperties(queryInfo.surface->GetNativeHandle());
                bool bSurfaceImageCountValid = surfaceProperties.maxImageCount > 1;
                bool bSurfaceFormatsValid = !surfaceProperties.availableSurfaceFormats.empty();
                bool bSurfacePresentModesValid = !surfaceProperties.availablePresentModes.empty();
                if (!bSurfaceImageCountValid || !bSurfaceFormatsValid || !bSurfacePresentModesValid)
                {
                    WARP_LOG_WARN(ELoggerType::NriLogger,
                                  "Physical device '{}' does not contain minimum support capabilities for surface provided during device creation. Skipping...",
                                  physicalDevice->GetInformation().deviceName);
                    continue;
                }
            }

            // iterate over all queue families to find at queues we are interested in.
            //
            // it is a good idea to use separate queues for graphics, async compute and transfer when available.
            // But some popular GPUs out there have only a single queue family with only one queue in it, so also account for that
            //
            // In Warp Engine we want to find a universal queue that supports graphics, compute and transfer capabilities,
            // as well as a separate compute-only queue and transfer-only queue.
            // If there are no compute-only or transfer-only queues, we will use the universal queue for those purposes as well.
            //
            // Acording to NVIDIAs convention as per https://vulkan.gpuinfo.org/displayreport.php?id=43769#queuefamilies, usually:
            // - queue family 0 would be considered a 'universal' queue family
            // - queue family 1 would support transfer-only (faster DMA probably?)
            // - queue family 2 would support compute and presentation

            NriSuitablePhysicalDeviceInfo potentialDeviceInfo;
            potentialDeviceInfo.physicalDevice = physicalDevice;
            potentialDeviceInfo.queueFamilyInfos.fill(NriPhysicalDeviceQueueFamilyInformation()); // initialize all queue family infos as invalid

            using DeviceQueueFamilyCriteria = std::function<bool(const NriPhysicalDeviceQueueFamilyInformation&)>;
            std::array<DeviceQueueFamilyCriteria, EnumValue(EDeviceQueueType::NumTypes)> isPerfectQueueFamily = {
                // Universal queue criteria. Checks whether a provided queue family info supports graphics, compute and transfer capabilities
                [physicalDevice, nativeSurface = queryInfo.surface->GetNativeHandle()](const NriPhysicalDeviceQueueFamilyInformation& queueFamilyInfo) -> bool
                {
                    return queueFamilyInfo.capabilities.AllOf(EQueueCapability::Graphics | EQueueCapability::Compute | EQueueCapability::Transfer) &&
                           physicalDevice->IsSurfaceSupportedByQueueFamily(nativeSurface,
                                                                           queueFamilyInfo); // also make sure to check if this queue supports surface
                },
                // Transfer queue criteria. Checks whether a provided queue family info supports transfer capability but not compute
                [](const NriPhysicalDeviceQueueFamilyInformation& queueFamilyInfo) -> bool
                { return queueFamilyInfo.capabilities.AllOf(EQueueCapability::Transfer) && queueFamilyInfo.capabilities.NoneOf(EQueueCapability::Compute); },
                // Compute queue criteria. Checks whether a provided queue family info supports compute capability but not graphics
                [](const NriPhysicalDeviceQueueFamilyInformation& queueFamilyInfo) -> bool
                { return queueFamilyInfo.capabilities.AllOf(EQueueCapability::Compute) && queueFamilyInfo.capabilities.NoneOf(EQueueCapability::Graphics); },
            };
            WARP_ASSERT(isPerfectQueueFamily.size() == potentialDeviceInfo.queueFamilyInfos.size(),
                        "Device queue family criteria array size does not match device queue types count");

            for (const NriPhysicalDeviceQueueFamilyInformation& queueFamilyInfo : physicalDevice->GetQueueFamilyInfoArray())
            {
                // So for each queue family information available for this physical device we would check for each device queue type
                // whether this queue family info satisfies the criteria for that device queue type
                //
                // For more information on device queue type iteration see EDeviceQueueType enumeration definition as well as 'isPerfectQueueFamily' array above
                for (uint32_t queueTypeIndex = 0; queueTypeIndex < potentialDeviceInfo.queueFamilyInfos.size(); ++queueTypeIndex)
                {
                    NriPhysicalDeviceQueueFamilyInformation& currentQueueFamilyInfo = potentialDeviceInfo.queueFamilyInfos.at(queueTypeIndex);
                    const DeviceQueueFamilyCriteria& isPerfectQueueFamilyCriteria = isPerfectQueueFamily.at(queueTypeIndex);
                    if (currentQueueFamilyInfo.IsInvalid() && isPerfectQueueFamilyCriteria(queueFamilyInfo))
                    {
                        currentQueueFamilyInfo = queueFamilyInfo;
                    }
                }
            }

            // Finally after selecting all possible queue families for each device queue type,
            // we also need to check if universal queue is present and patch transfer/compute queues if those were not found iteratively
            const NriPhysicalDeviceQueueFamilyInformation& universalQueueFamilyInfo =
                potentialDeviceInfo.queueFamilyInfos.at(EnumValue(EDeviceQueueType::Universal));
            if (universalQueueFamilyInfo.IsInvalid())
            {
                WARP_LOG_WARN(ELoggerType::NriLogger,
                              "Physical device '{}' did not provide queue families suitable for universal queues. Skipping...",
                              physicalDevice->GetInformation().deviceName);
                continue;
            }

            NriPhysicalDeviceQueueFamilyInformation& transferQueueFamilyInfo = potentialDeviceInfo.queueFamilyInfos.at(EnumValue(EDeviceQueueType::Transfer));
            if (transferQueueFamilyInfo.IsInvalid())
                transferQueueFamilyInfo = universalQueueFamilyInfo;

            NriPhysicalDeviceQueueFamilyInformation& computeQueueFamilyInfo = potentialDeviceInfo.queueFamilyInfos.at(EnumValue(EDeviceQueueType::Compute));
            if (computeQueueFamilyInfo.IsInvalid())
                computeQueueFamilyInfo = universalQueueFamilyInfo;

            // After all checks and queue family queries are done we can consider this physical device as suitable
            suitablePhysicalDeviceInfoArray.push_back(std::move(potentialDeviceInfo));
        }
        return suitablePhysicalDeviceInfoArray;
    }

    NriSuitablePhysicalDeviceInfo NriDevice::SelectBestSuitableDevice(std::span<const NriSuitablePhysicalDeviceInfo> allSuitablePhysicalDeviceInfos)
    {
        // after we have iterated over all physical devices, now we want to select one of the suitable devices to use for virtual device creation
        // To sort devices we calculate its priority based on its capabilities, such as device memory size and type
        struct PhysicalDevicePriority
        {
            inline constexpr bool operator<(const PhysicalDevicePriority& other) const noexcept { return priority < other.priority; }

            uint32_t suitablePhysicalDeviceIndex = uint32_t(-1);
            float priority = 0.0f;
        };

        // Calculate the priority of physical device based on its memory size and device type
        static constexpr auto GetDeviceTypePriority = [](VkPhysicalDeviceType type) -> float
        {
            switch (type)
            {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 4;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 3;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 2;
            case VK_PHYSICAL_DEVICE_TYPE_CPU: return 1;
            case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            default: return 0;
            }
        };

        static constexpr auto GetDeviceMemoryPriority = [](uint64_t totalMemoryInBytes) -> float
        {
            // to classify device priority based on memory we convert bytes to GB and assign priority accordingly
            float totalMemoryInGB = totalMemoryInBytes / (1024.0f * 1024.0f * 1024.0f); // convert to float and ceil to avoid 0
            return totalMemoryInGB;
        };

        std::priority_queue<PhysicalDevicePriority> physicalDevicePriorityQueue;
        for (uint32_t i = 0; i < allSuitablePhysicalDeviceInfos.size(); ++i)
        {
            const NriSuitablePhysicalDeviceInfo& suitablePhysicalDeviceInfo = allSuitablePhysicalDeviceInfos[i];

            Arc<NriPhysicalDevice> physicalDevice = suitablePhysicalDeviceInfo.physicalDevice;
            WARP_ASSERT(physicalDevice != nullptr && physicalDevice->GetNativeHandle() != VK_NULL_HANDLE, "NRI physical device was not initialized correctly");

            // Higher priority classifies better device
            const NriPhysicalDeviceInformation& deviceInfo = physicalDevice->GetInformation();
            const VkPhysicalDeviceMemoryProperties& memoryProperties = physicalDevice->GetMemoryProperties();

            // Calculate total memory size in bytes
            VkDeviceSize totalSizeInBytes = {};
            for (uint32_t heapIdx = 0; heapIdx < memoryProperties.memoryHeapCount; ++heapIdx)
            {
                totalSizeInBytes += memoryProperties.memoryHeaps[heapIdx].size;
            }

            physicalDevicePriorityQueue.push(
                PhysicalDevicePriority{ .suitablePhysicalDeviceIndex = i,
                                        .priority = GetDeviceTypePriority(deviceInfo.type) * 10.0f + // device type has higher weight
                                                    GetDeviceMemoryPriority(totalSizeInBytes) });
        }

        // check whether we found at least one suitable physical device
        // and return the best one from priority queue
        WARP_ASSERT(!physicalDevicePriorityQueue.empty(), "No suitable physical device found to create NRI logical device");
        return allSuitablePhysicalDeviceInfos[physicalDevicePriorityQueue.top().suitablePhysicalDeviceIndex];
    }

} // Warp::nri::vk namespace