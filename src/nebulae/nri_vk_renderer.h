#pragma once

#include "common/memory/arc.h"

#include "vulkan/nri_vk_common.h"
#include "vulkan/nri_vk_device.h"
#include "vulkan/nri_vk_swapchain.h"
#include "vulkan/nri_vk_shader_module.h"

namespace Warp::nri
{

    // TODO: Replace this with a render graph after a triangle is rendered
    class Renderer : public ArcMark<Renderer>
    {
    public:
        Renderer() = default;

        void Init();

        void NextFrame()
        {
            //vkAcquireNextImageKHR(m_device->GetNativeHandle(), m_swapchain->GetNativeHandle(), UINT64_MAX, )
        }

    private:
        Arc<vk::NriDevice> m_device;
        Arc<vk::NriSwapchain> m_swapchain;

        uint32_t m_frameIndex;
    };

} // Warp::nri namespace