#pragma once

#include "common/enum_flag_set.h"
#include "common/memory/arc.h"
#include "nri_vk_common.h"
#include "nri_vk_device.h"

#include <cstddef>

namespace Warp::nri::vk
{

struct NriBufferInfo
{
    /// @brief NRI device that will be used to create this buffer. Must not be null.
    Arc<NriDevice> device;

    /// @brief Optional flags for buffer creation. See VkBufferCreateFlagBits for more details on individual bits.
    VkBufferCreateFlags flags = 0;

    /// @brief Must be specified as a bitmask of VkBufferUsageFlagBits values. See VkBufferUsageFlagBits for more
    /// details on individual usage bits.
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;

    /// @brief Size of the buffer in bytes. Must be greater than 0.
    size_t sizeInBytes = 0;

    /// @brief Optional flags for memory allocation of this buffer. See VmaAllocationCreateFlagBits for more details on
    /// individual bits.
    VmaAllocationCreateFlags memoryAllocationFlags = VMA_ALLOCATION_CREATE_FLAG_BITS_MAX_ENUM;

    /// @brief Memory usage for this buffer, specified as VMA_MEMORY_USAGE enum value. See VMA_MEMORY_USAGE enum for
    /// more details on individual values.
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_UNKNOWN;

    /// @brief Optional set of queue types that will be sharing this buffer.
    /// If not specified, it is assumed that the buffer will only be used on a single queue type.
    FlagSet<EDeviceQueueType> concurrentSharingQueues;
};

class NriBuffer : public ArcMark<NriBuffer>
{
public:
    NriBuffer() = default;
    NriBuffer(const NriBufferInfo& bufferInfo);

    NriBuffer(const NriBuffer&) = delete;
    NriBuffer& operator=(const NriBuffer&) = delete;

    ~NriBuffer();

    inline constexpr VkBuffer GetNativeHandle() const noexcept { return m_nativeHandle; }
    inline constexpr VmaAllocation GetMemoryAllocation() const noexcept { return m_allocation; }
    inline constexpr size_t GetSizeInBytes() const noexcept { return m_info.sizeInBytes; }

    std::byte* MapMemoryToHost() const;
    void UnmapMemoryFromHost() const;

    /// @brief Only valid to call if NriBufferInfo::memoryAllocationFlags had VMA_ALLOCATION_CREATE_MAPPED_BIT set
    /// during creation. Returns a pointer to the host-mapped memory for this buffer. The returned pointer is valid for
    /// the entire lifetime of this buffer.
    std::byte* GetPersistentlyMappedMemory() const;

private:
    NriBufferInfo m_info = {};
    VkBuffer m_nativeHandle = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE; // VMA allocation handle, used for memory management of this buffer
};

} // namespace Warp::nri::vk