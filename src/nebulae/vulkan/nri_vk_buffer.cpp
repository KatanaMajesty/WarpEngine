#include "nri_vk_buffer.h"

namespace Warp::nri::vk
{

    NriBuffer::NriBuffer(const NriBufferInfo& bufferInfo)
        : m_info(bufferInfo)
    {
        // Sanity checks for buffer info
        WARP_ASSERT(bufferInfo.device != nullptr, "NRI buffer must have a valid device");
        WARP_ASSERT(bufferInfo.sizeInBytes > 0, "NRI buffer size must be greater than 0");
        WARP_ASSERT(bufferInfo.usage != VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM, "NRI buffer usage must be specified");

        // Create a buffer handle 
        auto createInfo = NRI_VK_STRUCT(VkBufferCreateInfo);
        createInfo.flags = m_info.flags;
        createInfo.size = m_info.sizeInBytes;
        createInfo.usage = m_info.usage;
        // might be empty, if concurrency is not required for this buffer, but if not empty, it must contain valid queue types
        // TODO: replace with fast_vector/inplace_vector
        std::vector<uint32_t> sharingQueueFamilyIndices;
        if (!m_info.concurrentSharingQueues.Any())
        {
            // If no bits are set in concurrentSharingQueues, 
            // we can assume that this buffer will only be used on a single queue type and use exclusive sharing mode
            createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        else
        {
            // if concurrency is required, we need to provide queue family indices of all queues 
            // that will be sharing this buffer and use concurrent sharing mode
            for (uint32_t queueTypeIndex = 0; queueTypeIndex < EnumValue(EDeviceQueueType::NumTypes); ++queueTypeIndex)
            {
                EDeviceQueueType queueType = static_cast<EDeviceQueueType>(queueTypeIndex);
                if (m_info.concurrentSharingQueues & queueType)
                {
                    sharingQueueFamilyIndices.push_back(m_info.device->GetQueueFamilyInfo(queueType).queueFamilyIndex);           
                }
            }
            createInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = static_cast<uint32_t>(sharingQueueFamilyIndices.size());
            createInfo.pQueueFamilyIndices = sharingQueueFamilyIndices.data();
        }
        WARP_ASSERT(bufferInfo.memoryAllocationFlags != VMA_ALLOCATION_CREATE_FLAG_BITS_MAX_ENUM,
                    "Incorrect memory allocation flags were specified for NRI buffer");
        WARP_ASSERT(bufferInfo.memoryUsage != VMA_MEMORY_USAGE_UNKNOWN, "Incorrect memory usage was specified for NRI buffer");

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.flags = bufferInfo.memoryAllocationFlags;
        allocCreateInfo.usage = bufferInfo.memoryUsage;
        allocCreateInfo.requiredFlags = 0; // leave at 0, as we use VmaMemoryUsage instead
        allocCreateInfo.preferredFlags = 0; // leave at 0, as we use VmaMemoryUsage instead
        allocCreateInfo.memoryTypeBits = UINT32_MAX; // set to UINT32_MAX to let VMA find suitable memory type index based on memory requirements of this buffer
                                                     // and memory usage specified in allocCreateInfo
        allocCreateInfo.pool = VK_NULL_HANDLE; // use default VMA pool
        allocCreateInfo.pUserData = nullptr;
        allocCreateInfo.priority = 0.0f; // ignored since we are not using memory priority feature of VMA
        NRI_VK_CHECK_RESULT(vmaCreateBuffer(m_info.device->GetMemoryAllocator(), &createInfo, &allocCreateInfo, &m_nativeHandle, &m_allocation, nullptr),
                            "Failed to create buffer and allocate memory using VMA");
    }

    NriBuffer::~NriBuffer()
    {
        if (m_nativeHandle != VK_NULL_HANDLE)
        {
            WARP_ASSERT(GetMemoryAllocation() != VK_NULL_HANDLE, "Buffer handle is valid, but not the memory allocation handle");
            vmaDestroyBuffer(m_info.device->GetMemoryAllocator(), m_nativeHandle, m_allocation);
        }
    }

    std::byte* NriBuffer::MapMemoryToHost() const
    {
        void* mappedData = nullptr;
        NRI_VK_CHECK_RESULT(vmaMapMemory(m_info.device->GetMemoryAllocator(), m_allocation, &mappedData), "Failed to map buffer memory to host");
        return static_cast<std::byte*>(mappedData);
    }

    void NriBuffer::UnmapMemoryFromHost() const
    {
        vmaUnmapMemory(m_info.device->GetMemoryAllocator(), m_allocation);
    }
   

} // namespace Warp::nri::vk