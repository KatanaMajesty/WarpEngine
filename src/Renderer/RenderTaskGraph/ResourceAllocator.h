#pragma once

#include <cstdint>
#include <concepts>
#include <vector>

#include "Defines.h"
#include "ResourceCommon.h"
#include "ResourceMemoryArena.h"
#include "../RHI/Resource.h"

namespace Warp
{

    // We always want to require memory arena's resource to be default-initializable
    template <typename T>
    concept IsRHIResource = (std::derived_from<T, RHIResource> && std::default_initializable<T>);

    // 26.03.24 -> Working on resource allocations. Basically, we want RTGResourceAllocator to derieve its behavior
    // from std::allocator struct but with more sugar on top to deal with RHI resources
    //
    template <IsRHIResource T>
    class RTGResourceAllocator
    {
    public:
        // Returns a proxy about the allocation that occured. If subsequent RTGResourceProxy::IsValid call returns true
        // this means that allocator was able to reserve the space for a resource that can be constructed using RTGResourceAllocator::Construct
        RTGResourceProxy Allocate();

        // Constructs an object of type T in allocated storage that is specified by RTGResourceProxy struct provided
        template <typename... Args>
        T* Construct(const RTGResourceProxy& proxy, Args&&... args);
        T* Get(const RTGResourceProxy& proxy);

        // Destroys an object of type T in allocated storage that is specified by RTGResourceProxy struct provided
        void Destroy(const RTGResourceProxy& proxy);

        // Deallocates the storage provided by RTGResourceProxy struct. Invalidates the provided RTGResourceProxy struct and all its copies
        void Deallocate(const RTGResourceProxy& proxy);

    private:
        static constexpr size_t MemoryArenaStride = sizeof(T);
        static constexpr size_t MemoryArenaNumElements = 128;

        bool Validate(const RTGResourceProxy& proxy) const;

        std::vector<std::unique_ptr<RTGMemoryArena>> m_Arenas;
    };

    template <IsRHIResource T>
    inline RTGResourceProxy RTGResourceAllocator<T>::Allocate()
    {
        RTGResourceProxy proxy = RTGResourceProxy();

        uint16_t arenaIndex = 0;
        for (; arenaIndex < m_Arenas.size(); ++arenaIndex)
        {
            RTGMemoryArena& arena = *m_Arenas[arenaIndex].get();
            RTGMemoryArena::AllocationResult result = arena.Allocate();
            if (result.AllocationIndex != RTGMemoryArena::InvalidIndex)
            {
                // We have successfully allocated!
                proxy = RTGResourceProxy{
                    .Version            = result.AllocationVersion,
                    .ArenaIndex         = arenaIndex,
                    .AllocationIndex    = result.AllocationIndex  
                };
                break;
            }
        }

        if (proxy.IsValid())
        {
            // Successfully created a proxy - return
            return proxy;
        }

        // If not allocated yet - create a new memory arena
        arenaIndex = static_cast<uint16_t>(m_Arenas.size());
        RTGMemoryArena& arena = *m_Arenas.emplace_back(new RTGMemoryArena(MemoryArenaStride, MemoryArenaNumElements)).get();
        RTGMemoryArena::AllocationResult result = arena.Allocate();
        if (result.AllocationIndex == RTGMemoryArena::InvalidIndex)
        {
            // Failed again - yield!
            WARP_RTG_LOG_ERROR("RTGResourceAllocator<T>::Allocate -> Could not allocate a resource proxy!");
            return proxy;
        }

        // We have successfully allocated!
        proxy = RTGResourceProxy{
            .Version            = result.AllocationVersion,
            .ArenaIndex         = arenaIndex,
            .AllocationIndex    = result.AllocationIndex
        };
        return proxy;
    }

    template <IsRHIResource T>
    template <typename... Args>
    inline T* RTGResourceAllocator<T>::Construct(const RTGResourceProxy& proxy, Args&&... args)
    {
        T* ptr = Get(proxy);
        if (!ptr)
        {
            return ptr;
        }

        T* t = new (ptr) T(std::forward<Args>(args)...);
        return t;
    }

    template <IsRHIResource T>
    inline T* RTGResourceAllocator<T>::Get(const RTGResourceProxy& proxy)
    {
        // TODO: In Get we might want to add some caching for constructed resources? Such that if the resource was constructed we would add it
        // to a hash table of some sort where we could query it. This would not guarantee better performance but extra safety as we would not
        // query unconstructed resources
        //
        // 26.03.24 -> I dont think this is the responsibility of allocator but rather of resource manager
            
        uint16_t numArenas = static_cast<uint16_t>(m_Arenas.size());
        if (!proxy.IsValid() || proxy.ArenaIndex >= numArenas)
        {
            return false;
        }
        
        return reinterpret_cast<T*>(m_Arenas[proxy.ArenaIndex]->GetAddress(proxy.AllocationIndex, proxy.Version));
    }

    template <IsRHIResource T>
    inline void RTGResourceAllocator<T>::Destroy(const RTGResourceProxy& proxy)
    {
        T* ptr = Get(proxy);
        if (ptr)
        {
            ptr->~T();
        }
    }

    template <IsRHIResource T>
    inline void RTGResourceAllocator<T>::Deallocate(const RTGResourceProxy& proxy)
    {
        uint16_t numArenas = static_cast<uint16_t>(m_Arenas.size());
        if (!proxy.IsValid() || proxy.ArenaIndex >= numArenas)
        {
            return false;
        }

        m_Arenas[proxy.ArenaIndex]->Release(proxy.AllocationIndex);
    }

    template <IsRHIResource T>
    inline bool RTGResourceAllocator<T>::Validate(const RTGResourceProxy& proxy) const
    {
        uint16_t numArenas = static_cast<uint16_t>(m_Arenas.size());
        if (!proxy.IsValid() || proxy.ArenaIndex >= numArenas)
        {
            return false;
        }

        return m_Arenas[proxy.ArenaIndex]->GetAddress(proxy.AllocationIndex, proxy.Version) != nullptr;
    }

}