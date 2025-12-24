#pragma once

#include "common/memory/arc.h"

#include "platform_window.h"

#include "vulkan/nri_vk_common.h"
#include "vulkan/nri_vk_device.h"
#include "vulkan/nri_vk_instance.h"
#include "vulkan/nri_vk_shader_module.h"
#include "vulkan/nri_vk_surface.h"
#include "vulkan/nri_vk_swapchain.h"

namespace Warp::nri
{

    struct RendererInfo
    {
        Arc<IPlatformWindow> window;
    };

    // TODO: Replace this with a render graph after a triangle is rendered
    class Renderer : public ArcMark<Renderer>
    {
    public:
        Renderer() = default;

        ~Renderer();

        void Init(const RendererInfo& info);

        void NextFrame()
        {
            //vkAcquireNextImageKHR(m_device->GetNativeHandle(), m_swapchain->GetNativeHandle(), UINT64_MAX, )
        }

    private:
        void InitInstance();
        void InitSurface(Arc<IPlatformWindow> window);
        void InitDevice();
        void InitSwapchain();
        Arc<vk::NriInstance> m_instance;
        Arc<vk::NriSurface> m_surface;
        Arc<vk::NriDevice> m_device;
        Arc<vk::NriSwapchain> m_swapchain;

        void InitShaderModules();
        VkShaderModule m_vsModule;
        VkShaderModule m_fsModule;

        uint32_t m_frameIndex;
    };

} // Warp::nri namespace