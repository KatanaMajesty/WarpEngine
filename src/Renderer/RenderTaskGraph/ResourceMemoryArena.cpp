#include "ResourceMemoryArena.h"

#include "Defines.h"

#include <algorithm>

namespace Warp
{

    RTGMemoryArena::RTGMemoryArena(uint32_t stride, uint32_t numElements)
        : m_Stride(stride)
        , m_Offset(0)
        , m_Bytes(static_cast<size_t>(numElements) * stride)
        , m_StateMap(numElements, MemoryState())
    {
        WARP_RTG_ASSERT(stride > 0);
        WARP_RTG_ASSERT(numElements < RTGMemoryArena::MaxNumElements);
    }

    RTGMemoryArena::AllocationResult RTGMemoryArena::Allocate()
    {
        uint32_t index = RTGMemoryArena::InvalidIndex;
        
        if (!m_ReleasedIndices.empty())
        {
            index = m_ReleasedIndices.front();
            m_ReleasedIndices.pop();
        }
        else
        {
            const size_t numElements = m_StateMap.size();
            if (m_Offset < numElements)
            {
                index = (m_Offset++);
            }
        }

        if (index == RTGMemoryArena::InvalidIndex)
        {
            // Yield if failed
            return AllocationResult();
        }

        WARP_RTG_ASSERT(IsReleased(index));
        m_StateMap[index].IsAllocated = true;

        return AllocationResult{
            .AllocationIndex    = index,
            .AllocationVersion  = ++m_StateMap[index].Version,
        };
    }

    void* RTGMemoryArena::GetAddress(uint32_t index, uint16_t version)
    {
        if (IsReleased(index) || !IsValidVersion(index, version))
        {
            return nullptr;
        }

        const size_t byteOffset = index * m_Stride;
        return m_Bytes.data() + byteOffset;
    }

    void RTGMemoryArena::Release(uint32_t index)
    {
        if (IsReleased(index))
        {
            return;
        }

        m_StateMap[index].IsAllocated = false;
        m_ReleasedIndices.push(index);

        const size_t byteOffset = index * m_Stride;
        auto begin = m_Bytes.begin() + byteOffset;
        auto end   = begin + m_Stride;
        std::fill(begin, end, std::byte(0));
    }

}