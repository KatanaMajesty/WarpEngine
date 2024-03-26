#pragma once

#include <cstdint>
#include <concepts>
#include <vector>
#include <queue>

#include "Defines.h"
#include "../RHI/Resource.h"

namespace Warp
{

    // TODO: 26.03.24 -> This should be probably moved to the utility category of the project
    // there is a caveat in this particular memory arena implementation which should be investigated
    // I didnt want to overcomplicate struct implementation thus is is a Fixed-Size memory arena
    // thus every allocation is fixed-sized and depends on the provided stride
    class RTGMemoryArena
    {
    public:
        static constexpr uint32_t MaxNumElements    = uint32_t(-1);
        static constexpr uint32_t InvalidIndex      = uint32_t(-1);

        RTGMemoryArena() = default;
        RTGMemoryArena(uint32_t stride, uint32_t numElements);

        RTGMemoryArena(const RTGMemoryArena&) = delete;
        RTGMemoryArena& operator=(const RTGMemoryArena&) = delete;

        RTGMemoryArena(RTGMemoryArena&&) = delete;
        RTGMemoryArena& operator=(RTGMemoryArena&&) = delete;

        struct AllocationResult
        {
            uint32_t AllocationIndex    = RTGMemoryArena::InvalidIndex;
            uint16_t AllocationVersion  = 0;
        };

        // Returns an index to an allocated (a.k.a. reserved) memory that can be retrieved using RTGMemoryArena::GetAddress
        AllocationResult Allocate();
        
        // Returns nullptr if the memory under the index is released or provided version is not correct
        void* GetAddress(uint32_t index, uint16_t version);

        // If the memory at index is allocated zeroes it, changes the state to EMemoryState::Released and adds the index to
        // released indices queue for the future allocations
        void Release(uint32_t index);

    private:
        bool IsAllocated    (uint32_t index) const { return m_StateMap.at(index).IsAllocated; }
        bool IsReleased     (uint32_t index) const { return !IsAllocated(index); }
        bool IsValidVersion (uint32_t index, uint16_t version) const { return m_StateMap.at(index).Version == version; }

        struct MemoryState
        {
            bool IsAllocated = false;
            uint16_t Version = 0;
        };

        size_t m_Stride = 0;
        size_t m_Offset = 0;
        std::queue<uint32_t> m_ReleasedIndices;

        std::vector<std::byte>      m_Bytes;
        std::vector<MemoryState>    m_StateMap; // for every element (not byte) stores a state of that particular memory slot, providing more safety
    };

}