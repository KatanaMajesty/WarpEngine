#pragma once

#include <cstdint>
#include <utility>

namespace Warp
{

    struct RTGResourceProxy
    {
        static constexpr uint16_t InvalidArenaIndex = uint16_t(-1);
        static constexpr uint32_t InvalidAllocationIndex = uint32_t(-1);

        // Allocation::IsValid should only be used on struct retrieval from RTGResourceAllocator::Allocate call
        // Otherwise it might not always mean allocation's validity, as it depends on whether or not the resource
        // under AllocationIndex was released or not using, potentially, the copy of this struct
        bool IsValid() const { return ArenaIndex != InvalidArenaIndex && AllocationIndex != InvalidAllocationIndex; }

        // Version indicates of a uniqueness of a resource at particular index
        // Basically there should almost certainly be no case where two Allocation structs
        // that were returned from allocator would have the same version for the same index
        uint16_t Version = 0;
        uint16_t ArenaIndex = InvalidArenaIndex;
        uint32_t AllocationIndex = InvalidAllocationIndex;
    };

}

namespace std
{
    template<>
    struct hash<Warp::RTGResourceProxy>
    {
        constexpr std::size_t operator()(const Warp::RTGResourceProxy& proxy) const
        {
            std::size_t r = 0;
            HashCombine(r, proxy.Version);
            HashCombine(r, proxy.ArenaIndex);
            HashCombine(r, proxy.AllocationIndex);
            return r;
        }

    private:
        // We like doing what boost once did
        // TODO: Maybe this should be a separate piece of art?
        template <typename T>
        static constexpr void HashCombine(std::size_t& seed, const T& t)
        {
            seed ^= std::hash<T>()(t) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
    };
}