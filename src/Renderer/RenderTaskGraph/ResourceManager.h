#pragma once

#include <unordered_map>
#include <type_traits>

#include "ResourceCommon.h"
#include "ResourceAllocator.h"

namespace Warp
{

    class RTGResourceManager
    {
    public:
        RTGResourceManager() = default;
        
        // SubmitResource is a somewhat confusing name. We basically defer the resource creation until GetResource is called. Figure out the better name
        RTGResourceProxy SubmitResource(const D3D12_RESOURCE_DESC& desc);

        // As it sounds
        void ReleaseResource(const RTGResourceProxy& proxy);

        // This should generally be only called from execution callback of render tasks
        template<IsRHIResource T>
        T* GetResource(const RTGResourceProxy& proxy) const
        {
            // check what type is it
            // TODO: Move this, was just playing around (26.03.24)
            if constexpr (std::is_same_v<T, RHITexture>)
            {
                // check whether or not is constructed
                auto it = m_DeferredResources.find(proxy);
                if (it != m_DeferredResources.end())
                {
                    // Lazy-Construct
                    const D3D12_RESOURCE_DESC& desc = it->second;
                    return m_TextureAllocator.Construct(proxy, desc /*desc, but should be more*/);
                }
                return m_TextureAllocator.Get(proxy);
            }
            
            if constexpr (std::is_same_v<T, RHIBuffer>)
            {
                // Same here... just playing around
            }
        }

    private:
        std::unordered_map<RTGResourceProxy, D3D12_RESOURCE_DESC> m_DeferredResources;

        RTGResourceAllocator<RHITexture> m_TextureAllocator;
        RTGResourceAllocator<RHIBuffer>  m_BufferAllocator;
    };

}