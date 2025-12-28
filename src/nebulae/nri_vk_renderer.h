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

        /// @brief An amount of frames to be submitted inflight.
        /// For X frames in flight X frames will be submitted by the CPU to GPU before waiting for first submitted frame to finish
        uint32_t numFramesInFlight = 0;
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

        void Resize();

        // TODO: replace this with NextFrame?
        void RenderFrame(); 
        void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t swapchainImageIndex);

    private:
        RendererInfo m_info;
        uint32_t m_frameIndex = 0;

        void InitInstance();
        void InitSurface(Arc<IPlatformWindow> window);
        void InitDevice();
        void InitSwapchain();
        Arc<vk::NriInstance> m_instance;
        Arc<vk::NriSurface> m_surface;
        Arc<vk::NriDevice> m_device;
        Arc<vk::NriSwapchain> m_swapchain;

        void InitTriangleShaderModules();
        VkShaderModule m_vsModule = VK_NULL_HANDLE;
        VkShaderModule m_fsModule = VK_NULL_HANDLE;
        
        void InitGraphicsTriangleRenderPass();
        VkRenderPass m_triangleRenderPass = VK_NULL_HANDLE;

        void InitGraphicsTrianglePipe();
        VkPipelineLayout m_triangleLayout = VK_NULL_HANDLE;
        VkPipeline m_trianglePipe = VK_NULL_HANDLE;

        void InitFramebuffers();
        std::vector<VkFramebuffer> m_swapchainFramebuffers;

        void InitTriangleCommandBuffers();
        std::vector<VkCommandBuffer> m_commandBuffers;

        void InitSyncPrimitives();
        std::vector<VkSemaphore> m_imageAvailableSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::vector<VkFence> m_inflightFences;
    };

} // Warp::nri namespace