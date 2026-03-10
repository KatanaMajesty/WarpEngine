#pragma once

#include <vulkan/vulkan.h>

namespace Warp::nri::vk
{

    class NriMemoryAllocator
    {
    public:
        NriMemoryAllocator() = default;

    private:
        /// @brief 
        /// If pfnAllocation is unable to allocate the requested memory, it must return NULL. 
        /// If the allocation was successful, it must return a valid pointer to memory allocation containing at least size bytes, 
        /// and with the pointer value being a multiple of alignment.
        void* Allocate(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);

        /// @brief Vulkan memory reallocation function.
        /// 
        /// If the reallocation was successful, pfnReallocation must return an allocation with enough space for size bytes, 
        /// and the contents of the original allocation from bytes zero to min(original size, new size) - 1 must be preserved in the returned allocation. 
        /// 
        /// If size is larger than the old size, the contents of the additional space are undefined. 
        /// If satisfying these requirements involves creating a new allocation, then the old allocation should be freed.
        void* Reallocate(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);

        /// @brief pMemory may be NULL, which the callback must handle safely. 
        /// If pMemory is non-NULL, it must be a pointer previously allocated by pfnAllocation or pfnReallocation. 
        /// The application should free this memory.
        void Free(void* pUserData, void* pMemory);
    };

} // Warp::nri::vk namespace