#pragma once

#include "../../Core/Defines.h"
#include "../../Core/Assert.h"

namespace Warp
{

    class RHIDevice;

    class RHIDeviceChild
    {
    public:
        RHIDeviceChild() = default;
        explicit RHIDeviceChild(RHIDevice* device)
            : m_device(device)
        {
            WARP_ASSERT(device);
        }

        // Returns a logical device, associated with the child
        WARP_ATTR_NODISCARD inline constexpr RHIDevice* GetDevice() const { return m_device; }

    protected:
        RHIDevice* m_device;
    };

}