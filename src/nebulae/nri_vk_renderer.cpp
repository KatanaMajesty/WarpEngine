#include "nri_vk_renderer.h"

#include "common/assert.h"
#include "common/attr_defs.h"
#include "shader_factory/nri_shader_library.h"

#include <span>

namespace Warp::nri
{

    Renderer::~Renderer()
    {
        // before destroying anything wait on device to finish outstanding jobs
        m_device->WaitIdle();

        const uint32_t numVertexAttributes = EnumValue(EVertexAttributeIndex::Last);
        for (uint32_t i = 0; i < numVertexAttributes; ++i)
        {
            vkDestroyBuffer(m_device->GetNativeHandle(), m_vertexAttributeBuffers.at(i), nullptr);
            vkFreeMemory(m_device->GetNativeHandle(), m_vertexAttributeMemories.at(i), nullptr);
        }

        // destroy sync primitives
        const uint32_t numFramesInFlight = m_info.numFramesInFlight;
        for (uint32_t inflightFrameIndex = 0; inflightFrameIndex < numFramesInFlight; ++inflightFrameIndex)
        {
            vkDestroyFence(m_device->GetNativeHandle(), m_inflightFences.at(inflightFrameIndex), nullptr);
            vkDestroySemaphore(m_device->GetNativeHandle(), m_renderFinishedSemaphores.at(inflightFrameIndex), nullptr);
            vkDestroySemaphore(m_device->GetNativeHandle(), m_imageAvailableSemaphores.at(inflightFrameIndex), nullptr);
        }

        // destroy framebuffers
        for (VkFramebuffer framebuffer : m_swapchainFramebuffers)
        {
            vkDestroyFramebuffer(m_device->GetNativeHandle(), framebuffer, nullptr);
        }

        // destroy pipelines & layouts
        vkDestroyPipeline(m_device->GetNativeHandle(), m_trianglePipe, nullptr);
        vkDestroyPipelineLayout(m_device->GetNativeHandle(), m_triangleLayout, nullptr);

        // destroy render passes
        vkDestroyRenderPass(m_device->GetNativeHandle(), m_triangleRenderPass, nullptr);

        // destroy shader modules
        vkDestroyShaderModule(m_device->GetNativeHandle(), m_fsModule, nullptr);
        vkDestroyShaderModule(m_device->GetNativeHandle(), m_vsModule, nullptr);
    }

    void Renderer::Init(const RendererInfo& info)
    {
        m_info = info;
        WARP_ASSERT(info.numFramesInFlight > 0, "Invalid number of in-flight frames. Must be > 0");
        m_frameIndex = 0;

        InitInstance();
        InitSurface(info.window);
        InitDevice();
        InitSwapchain();

        // shader module initialization
        InitTriangleShaderModules();
        InitGraphicsTriangleRenderPass();
        InitGraphicsTrianglePipe();

        // framebuffer & cb initialization
        InitFramebuffers();
        InitTriangleCommandBuffers();
        InitSyncPrimitives();

        // vertex buffer initialization
        InitTriangleVertexBuffers();
    }

    void Renderer::Resize()
    {
        // before destroying anything wait on device to finish outstanding jobs
        m_device->WaitIdle();

        m_swapchain->HandleResize();

        // destroy render passes
        vkDestroyRenderPass(m_device->GetNativeHandle(), m_triangleRenderPass, nullptr);
        // after resizing swapchain recreate render passes and framebuffers.
        // we also recreate render passes because swapchain format might have changed after resize
        this->InitGraphicsTriangleRenderPass();

        // destroy framebuffers
        for (VkFramebuffer framebuffer : m_swapchainFramebuffers)
        {
            vkDestroyFramebuffer(m_device->GetNativeHandle(), framebuffer, nullptr);
        }
        this->InitFramebuffers();
    }

    void Renderer::RenderFrame()
    {
        const uint32_t frameIndex = m_frameIndex;
        VkFence inflightFence = m_inflightFences.at(frameIndex);
        VkSemaphore renderFinishedSemaphore = m_renderFinishedSemaphores.at(frameIndex);
        VkSemaphore imageAvailableSemaphore = m_imageAvailableSemaphores.at(frameIndex);
        VkCommandBuffer commandBuffer = m_commandBuffers.at(frameIndex);

        NRI_VK_CHECK_RESULT(vkWaitForFences(m_device->GetNativeHandle(), 1, &inflightFence, VK_TRUE, UINT64_MAX), "Failed to wait for inflight fence");
        
        if (!m_swapchain->AcquireNextImage(imageAvailableSemaphore))
        {
            // If AcquireNext image returns false this means swapchain is incompatible
            // in such case we need to immediately handle swapchain recreation and bail out of this frame render
            Resize();
            return;
        }
        const uint32_t swapchainImageIndex = m_swapchain->GetCurrentImageIndex();

        // after waiting for fences we need to manually reset them
        // also important to only reset fences if no resize should have occured during this frame! So after acquiring an image!
        NRI_VK_CHECK_RESULT(vkResetFences(m_device->GetNativeHandle(), 1, &inflightFence), "Failed to reset inflight fence");
        NRI_VK_CHECK_RESULT(vkResetCommandBuffer(commandBuffer, 0 /*no flags*/), "Failed to reset command buffer");
        RecordCommandBuffer(commandBuffer, swapchainImageIndex);

        // Only wait for pipe stage when color attachment write would begin,
        // this potentially allows driver to start executing other stages, such as vertex shaders
        VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        auto submitInfo = NRI_VK_STRUCT(VkSubmitInfo);
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
        submitInfo.pWaitDstStageMask = &waitStageMask;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphore;
        NRI_VK_CHECK_RESULT(vkQueueSubmit(m_device->GetQueue(vk::EDeviceQueueType::Universal), 1, &submitInfo, inflightFence),
                            "Failed to submit queue commands");

        VkSwapchainKHR nativeSwapchain = m_swapchain->GetNativeHandle();
        auto presentInfo = NRI_VK_STRUCT(VkPresentInfoKHR);
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &nativeSwapchain;
        presentInfo.pImageIndices = &swapchainImageIndex;
        presentInfo.pResults = nullptr; // not used as we only use 1 swapchain

        NRI_VK_CHECK_RESULT(vkQueuePresentKHR(m_device->GetQueue(vk::EDeviceQueueType::Universal), &presentInfo), "Failed to present swapchain");

        m_frameIndex = (m_frameIndex + 1) % m_info.numFramesInFlight;
    }

    void Renderer::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t swapchainImageIndex)
    {
        auto commandBufferBeginInfo = NRI_VK_STRUCT(VkCommandBufferBeginInfo);
        commandBufferBeginInfo.flags = 0;                  // not used for now
        commandBufferBeginInfo.pInheritanceInfo = nullptr; // not used
        NRI_VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo), "Failed to begin command buffer");
        {
            VkClearValue clearValue;
            clearValue.color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clearValue.depthStencil.depth = 0.0f;
            clearValue.depthStencil.stencil = 0;

            // command submition scope
            auto beginRenderPassInfo = NRI_VK_STRUCT(VkRenderPassBeginInfo);
            beginRenderPassInfo.renderPass = m_triangleRenderPass;
            beginRenderPassInfo.framebuffer = m_swapchainFramebuffers.at(swapchainImageIndex);
            beginRenderPassInfo.renderArea = { .offset = { 0, 0 }, .extent = m_swapchain->GetCurrentExtent() };
            beginRenderPassInfo.clearValueCount = 1;
            beginRenderPassInfo.pClearValues = &clearValue;
            // Use VK_SUBPASS_CONTENTS_INLINE, because
            // The render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed
            vkCmdBeginRenderPass(commandBuffer, &beginRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_trianglePipe);

            // Bind vertex buffer information to the pipe
            {
                const uint32_t numVertexAttributes = EnumValue(EVertexAttributeIndex::Last);
                std::array dummyOffsets1 = { VkDeviceSize{}, VkDeviceSize{} };
                vkCmdBindVertexBuffers(commandBuffer, 0, numVertexAttributes, m_vertexAttributeBuffers.data(), dummyOffsets1.data());
            }

            // TODO: remove this and instead use VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT && VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT
            VkViewport viewport = { .x = 0,
                                    .y = 0,
                                    .width = static_cast<float>(m_swapchain->GetWidth()),
                                    .height = static_cast<float>(m_swapchain->GetHeight()),
                                    .minDepth = 0.0f,
                                    .maxDepth = 1.0f };
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissorRect = { .offset = VkOffset2D{ 0, 0 }, .extent = m_swapchain->GetCurrentExtent() };
            vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);

            // submit drawcall
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
            vkCmdEndRenderPass(commandBuffer);
        }
        NRI_VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer), "Failed to end command buffer");
    }

    void Renderer::InitInstance()
    {
        m_instance = Arc<vk::NriInstance>::Make(vk::NriInstanceInfo{
            .appName = "Warp test application",
            .appVersion = VK_MAKE_API_VERSION(1, 0, 0, 0),
            .engineName = "Warp Engine",
            .engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0),
            .bSurfaceRequired = true,
        });
    }

    void Renderer::InitSurface(Arc<IPlatformWindow> window)
    {
        vk::ESurfaceType surfaceType = vk::ESurfaceType::None;
        switch (window->GetType())
        {
        case EWindowImpl::Win32: surfaceType = vk::ESurfaceType::Win32; break;
        case EWindowImpl::None: WARP_A_FALLTHROUGH;
        default: WARP_ASSERT(false, "Unknown window implementation type specified");
        }

        m_surface = Arc<vk::NriSurface>::Make(vk::NriSurfaceInfo{
            .instance = m_instance,
            .type = surfaceType,
            .nativeHandle = window->GetNativeHandle(),
        });
    }

    void Renderer::InitDevice()
    {
        m_device = Arc<vk::NriDevice>::Make(vk::NriDeviceInfo{
            .instance = m_instance,
            .surface = m_surface,
            .bSwapchainRequired = true,
        });
    }

    void Renderer::InitSwapchain()
    {
        m_swapchain = Arc<vk::NriSwapchain>::Make(vk::NriSwapchainInfo{
            .surface = m_surface,
            .device = m_device,
            .numSwapchainImages = 3,
        });
    }

    void Renderer::InitTriangleShaderModules()
    {
        static auto CreateShaderModule = [nativeDevice = m_device->GetNativeHandle()](std::span<const uint32_t> spirvWords)
        {
            auto createInfo = NRI_VK_STRUCT(VkShaderModuleCreateInfo);
            createInfo.codeSize = spirvWords.size_bytes();
            createInfo.pCode = spirvWords.data();

            VkShaderModule shaderModule;
            NRI_VK_CHECK_RESULT(vkCreateShaderModule(nativeDevice, &createInfo, nullptr, &shaderModule), "Failed to create shader module");
            return shaderModule;
        };

        ShaderLibrary* shaderLibrary = ShaderLibrary::Get();
        ShaderCompilerOutput vsHelloTriangleSpirv = shaderLibrary->CompileFromLibrary(EShaderLang::Slang, "hello_triangle_vb.slang", "vertexMain", {});
        ShaderCompilerOutput fsHelloTriangleSpirv = shaderLibrary->CompileFromLibrary(EShaderLang::Slang, "hello_triangle_vb.slang", "fragmentMain", {});

        m_vsModule = CreateShaderModule(vsHelloTriangleSpirv.spirvWords);
        m_fsModule = CreateShaderModule(fsHelloTriangleSpirv.spirvWords);
    }

    void Renderer::InitGraphicsTriangleRenderPass()
    {
        auto attachmentInfo = NRI_VK_STRUCT(VkAttachmentDescription2);
        attachmentInfo.flags = 0; // https://docs.vulkan.org/refpages/latest/refpages/source/VkAttachmentDescriptionFlagBits.html
        attachmentInfo.format = m_swapchain->GetSurfaceFormat().format;
        attachmentInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentInfo.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;   // we dont care about stencil here
        attachmentInfo.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // we dont care about stencil here
        // For image layouts see ref https://docs.vulkan.org/refpages/latest/refpages/source/VkImageLayout.html
        attachmentInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentInfo.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        auto attachmentReference = NRI_VK_STRUCT(VkAttachmentReference2);
        // either an integer value identifying an attachment at the corresponding index in VkRenderPassCreateInfo2::pAttachments,
        // or VK_ATTACHMENT_UNUSED to signify that this attachment is not used
        attachmentReference.attachment = 0;
        attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        // ignored when this structure is used to describe anything other than an input attachment reference
        attachmentReference.aspectMask = 0;

        // VkSubpassDescription2 is superseded by Vulkan Version 1.4. See Legacy Functionality for more information.
        // ref for Vk 1.4: https://docs.vulkan.org/spec/latest/appendices/versions.html#versions-1.4
        auto subpassInfo = NRI_VK_STRUCT(VkSubpassDescription2);
        subpassInfo.flags = 0;
        subpassInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassInfo.viewMask = 0; // bitfield of view indices describing which views are active during rendering, when it is not 0
        // no input attachments
        subpassInfo.inputAttachmentCount = 0;
        subpassInfo.pInputAttachments = nullptr;
        // only 1 color attachment for swapchain
        subpassInfo.colorAttachmentCount = 1;
        subpassInfo.pColorAttachments = &attachmentReference;
        // no resolve attachments
        subpassInfo.pResolveAttachments = nullptr;
        // no depth-stencil attachments
        subpassInfo.pDepthStencilAttachment = nullptr;
        // no preserve attachments
        subpassInfo.preserveAttachmentCount = 0;
        subpassInfo.pPreserveAttachments = nullptr;

        // If VK_SUBPASS_EXTERNAL the first synchronization scope includes commands that occur earlier in submission order
        // than the vkCmdBeginRenderPass used to begin the render pass instance
        auto subpassDependencyInfo = NRI_VK_STRUCT(VkSubpassDependency2);
        subpassDependencyInfo.srcSubpass = VK_SUBPASS_EXTERNAL; // Subpass index sentinel expanding synchronization scope outside a subpass
        subpassDependencyInfo.dstSubpass = 0;
        subpassDependencyInfo.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependencyInfo.srcAccessMask = 0;
        subpassDependencyInfo.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependencyInfo.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        subpassDependencyInfo.dependencyFlags = 0; // not used
        subpassDependencyInfo.viewOffset = 0;      // not used

        auto renderPassInfo = NRI_VK_STRUCT(VkRenderPassCreateInfo2);
        renderPassInfo.flags = 0;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &attachmentInfo;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpassInfo;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &subpassDependencyInfo;
        NRI_VK_CHECK_RESULT(vkCreateRenderPass2(m_device->GetNativeHandle(), &renderPassInfo, nullptr, &m_triangleRenderPass), "Failed to create render pass");
    }

    void Renderer::InitGraphicsTrianglePipe()
    {
        // when compiled to SPIR-V, shader module will keep entrypoint name as 'main' to match defaults found in other shading langauges such as GLSL.
        // It is also valid in a single SPIR-V binary to have 'main' for two different stages.
        auto vsShaderStageInfo = NRI_VK_STRUCT(VkPipelineShaderStageCreateInfo);
        vsShaderStageInfo.flags = 0;
        vsShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vsShaderStageInfo.module = m_vsModule;
        vsShaderStageInfo.pName = "main";
        vsShaderStageInfo.pSpecializationInfo = nullptr;

        auto fsShaderStageInfo = NRI_VK_STRUCT(VkPipelineShaderStageCreateInfo);
        fsShaderStageInfo.flags = 0;
        fsShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fsShaderStageInfo.module = m_fsModule;
        fsShaderStageInfo.pName = "main";
        fsShaderStageInfo.pSpecializationInfo = nullptr;

        std::array shaderStageInfos = { vsShaderStageInfo, fsShaderStageInfo };

        std::array vertexBindingDescriptions = {
            m_triangleVertexCollection.GetBindingDescription(EVertexAttributeIndex::Position, true),
            m_triangleVertexCollection.GetBindingDescription(EVertexAttributeIndex::Color, true)
        };

        std::array vertexAttributeDescriptions = {
            m_triangleVertexCollection.GetAttributeDescription(EVertexAttributeIndex::Position, 0 /*location*/),
            m_triangleVertexCollection.GetAttributeDescription(EVertexAttributeIndex::Color, 1 /*location*/),
        };

        auto pipeVertexInputInfo = NRI_VK_STRUCT(VkPipelineVertexInputStateCreateInfo);
        // TODO: proper vertex state
        pipeVertexInputInfo.flags = 0;
        pipeVertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindingDescriptions.size());
        pipeVertexInputInfo.pVertexBindingDescriptions = vertexBindingDescriptions.data();
        pipeVertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size());
        pipeVertexInputInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();

        auto pipeInputAssemblyInfo = NRI_VK_STRUCT(VkPipelineInputAssemblyStateCreateInfo);
        pipeInputAssemblyInfo.flags = 0;
        pipeInputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipeInputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

        // TODO: remove this and instead use VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT && VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT
        VkViewport pipeViewport = { .x = 0,
                                    .y = 0,
                                    .width = static_cast<float>(m_swapchain->GetWidth()),
                                    .height = static_cast<float>(m_swapchain->GetHeight()),
                                    .minDepth = 0.0f,
                                    .maxDepth = 1.0f };
        VkRect2D pipeScissorRect = { .offset = VkOffset2D{ 0, 0 }, .extent = m_swapchain->GetCurrentExtent() };
        auto pipeViewportInfo = NRI_VK_STRUCT(VkPipelineViewportStateCreateInfo);
        pipeViewportInfo.viewportCount = 1;
        pipeViewportInfo.pViewports = &pipeViewport;
        pipeViewportInfo.scissorCount = 1;
        pipeViewportInfo.pScissors = &pipeScissorRect;

        auto pipeRasterizationInfo = NRI_VK_STRUCT(VkPipelineRasterizationStateCreateInfo);
        pipeRasterizationInfo.flags = 0;
        pipeRasterizationInfo.depthClampEnable = VK_FALSE;
        pipeRasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
        pipeRasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
        pipeRasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        pipeRasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        pipeRasterizationInfo.depthBiasEnable = VK_FALSE;
        pipeRasterizationInfo.depthBiasConstantFactor = 0.0f;
        pipeRasterizationInfo.depthBiasClamp = 0.0f;
        pipeRasterizationInfo.depthBiasSlopeFactor = 0.0f;
        pipeRasterizationInfo.lineWidth = 1.0f;

        auto pipeMultisampleInfo = NRI_VK_STRUCT(VkPipelineMultisampleStateCreateInfo);
        pipeMultisampleInfo.flags = 0;
        pipeMultisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        pipeMultisampleInfo.sampleShadingEnable = VK_FALSE;
        pipeMultisampleInfo.minSampleShading = 1.0f;
        pipeMultisampleInfo.pSampleMask = nullptr;
        pipeMultisampleInfo.alphaToCoverageEnable = VK_FALSE;
        pipeMultisampleInfo.alphaToOneEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState attachmentBlendState;
        attachmentBlendState.blendEnable = VK_FALSE;
        attachmentBlendState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        attachmentBlendState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachmentBlendState.colorBlendOp = VK_BLEND_OP_ADD;
        attachmentBlendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachmentBlendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachmentBlendState.alphaBlendOp = VK_BLEND_OP_ADD;
        attachmentBlendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        auto pipeBlendState = NRI_VK_STRUCT(VkPipelineColorBlendStateCreateInfo);
        pipeBlendState.flags = 0;
        pipeBlendState.logicOpEnable = VK_FALSE;
        pipeBlendState.logicOp = VK_LOGIC_OP_COPY; // not used as VK_FALSE above
        pipeBlendState.attachmentCount = 1;
        pipeBlendState.pAttachments = &attachmentBlendState;
        pipeBlendState.blendConstants[0] = 0.0f;
        pipeBlendState.blendConstants[1] = 0.0f;
        pipeBlendState.blendConstants[2] = 0.0f;
        pipeBlendState.blendConstants[3] = 0.0f;

        std::array pipeDynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        auto pipeDynamicStateInfo = NRI_VK_STRUCT(VkPipelineDynamicStateCreateInfo);
        pipeDynamicStateInfo.flags = 0;
        pipeDynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(pipeDynamicStates.size());
        pipeDynamicStateInfo.pDynamicStates = pipeDynamicStates.data();

        auto pipeLayoutInfo = NRI_VK_STRUCT(VkPipelineLayoutCreateInfo);
        pipeLayoutInfo.setLayoutCount = 0;
        pipeLayoutInfo.pSetLayouts = nullptr;
        pipeLayoutInfo.pushConstantRangeCount = 0;
        pipeLayoutInfo.pPushConstantRanges = nullptr;
        NRI_VK_CHECK_RESULT(vkCreatePipelineLayout(m_device->GetNativeHandle(), &pipeLayoutInfo, nullptr, &m_triangleLayout),
                            "Failed to create pipeline layout");

        auto graphicsPipeCreateInfo = NRI_VK_STRUCT(VkGraphicsPipelineCreateInfo);
        graphicsPipeCreateInfo.stageCount = static_cast<uint32_t>(shaderStageInfos.size());
        graphicsPipeCreateInfo.pStages = shaderStageInfos.data();
        graphicsPipeCreateInfo.pVertexInputState = &pipeVertexInputInfo;
        graphicsPipeCreateInfo.pInputAssemblyState = &pipeInputAssemblyInfo;
        graphicsPipeCreateInfo.pTessellationState = nullptr; // not used
        // TODO: Should this be nullptr?
        // With might wanna use VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT or VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT
        graphicsPipeCreateInfo.pViewportState = &pipeViewportInfo;
        graphicsPipeCreateInfo.pRasterizationState = &pipeRasterizationInfo;
        graphicsPipeCreateInfo.pMultisampleState = &pipeMultisampleInfo;
        graphicsPipeCreateInfo.pDepthStencilState = nullptr; // not used
        graphicsPipeCreateInfo.pColorBlendState = &pipeBlendState;
        graphicsPipeCreateInfo.pDynamicState = &pipeDynamicStateInfo;
        // TODO: layouts, render passes, subpasses
        graphicsPipeCreateInfo.layout = m_triangleLayout;
        graphicsPipeCreateInfo.renderPass = m_triangleRenderPass;
        graphicsPipeCreateInfo.subpass = 0;
        graphicsPipeCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // not used
        graphicsPipeCreateInfo.basePipelineIndex = -1;              // not used
        NRI_VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device->GetNativeHandle(),
                                                      VK_NULL_HANDLE, // pipeline cache not used
                                                      1,
                                                      &graphicsPipeCreateInfo,
                                                      nullptr,
                                                      &m_trianglePipe),
                            "Failed to create graphics pipeline");
    }

    void Renderer::InitFramebuffers()
    {
        m_swapchainFramebuffers.resize(m_swapchain->GetNumSwapchainImages());
        for (uint32_t swapchainImageIndex = 0; swapchainImageIndex < m_swapchain->GetNumSwapchainImages(); ++swapchainImageIndex)
        {
            VkImageView attachmentImageView = m_swapchain->GetSwapchainImageView(swapchainImageIndex);

            auto framebufferInfo = NRI_VK_STRUCT(VkFramebufferCreateInfo);
            framebufferInfo.flags = 0;
            framebufferInfo.renderPass = m_triangleRenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = &attachmentImageView;
            framebufferInfo.width = m_swapchain->GetWidth();
            framebufferInfo.height = m_swapchain->GetHeight();
            framebufferInfo.layers = 1;

            NRI_VK_CHECK_RESULT(vkCreateFramebuffer(m_device->GetNativeHandle(), &framebufferInfo, nullptr, &m_swapchainFramebuffers.at(swapchainImageIndex)),
                                "Failed to create framebuffer for swapchain image {}",
                                swapchainImageIndex);
        }
    }

    void Renderer::InitTriangleCommandBuffers()
    {
        m_commandBuffers.resize(m_info.numFramesInFlight);

        auto commandBufferInfo = NRI_VK_STRUCT(VkCommandBufferAllocateInfo);
        commandBufferInfo.commandPool = m_device->GetCommandPool(vk::EDeviceQueueType::Universal);
        commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());
        NRI_VK_CHECK_RESULT(vkAllocateCommandBuffers(m_device->GetNativeHandle(), &commandBufferInfo, m_commandBuffers.data()),
                            "Failed to create command buffer");
    }

    void Renderer::InitSyncPrimitives()
    {
        const uint32_t numFramesInFlight = m_info.numFramesInFlight;
        m_imageAvailableSemaphores.resize(numFramesInFlight);
        m_renderFinishedSemaphores.resize(numFramesInFlight);
        m_inflightFences.resize(numFramesInFlight);

        for (uint32_t inflightFrameIndex = 0; inflightFrameIndex < numFramesInFlight; ++inflightFrameIndex)
        {
            auto imageAvailableSemaphoreInfo = NRI_VK_STRUCT(VkSemaphoreCreateInfo);
            NRI_VK_CHECK_RESULT(
                vkCreateSemaphore(m_device->GetNativeHandle(), &imageAvailableSemaphoreInfo, nullptr, &m_imageAvailableSemaphores.at(inflightFrameIndex)),
                "Failed to create image available semaphore");

            auto renderFinishedSemaphoreInfo = NRI_VK_STRUCT(VkSemaphoreCreateInfo);
            NRI_VK_CHECK_RESULT(
                vkCreateSemaphore(m_device->GetNativeHandle(), &renderFinishedSemaphoreInfo, nullptr, &m_renderFinishedSemaphores.at(inflightFrameIndex)),
                "Failed to create render finished semaphore");

            // first frame will wait infinitely for unsignalled fence, so create it signalled
            auto inflightFenceInfo = NRI_VK_STRUCT(VkFenceCreateInfo);
            inflightFenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            NRI_VK_CHECK_RESULT(vkCreateFence(m_device->GetNativeHandle(), &inflightFenceInfo, nullptr, &m_inflightFences.at(inflightFrameIndex)),
                                "Failed to create in-flight fence");
        }
    }

    void Renderer::InitTriangleVertexBuffers()
    {
        WARP_ASSERT(m_device && m_device->GetPhysicalDevice(), "Physical device is not reachable!");

        const uint32_t numAttributes = EnumValue(EVertexAttributeIndex::Last);
        /* destroy previously allocated buffers, if any */
        for (uint32_t i = 0; i < numAttributes; ++i)
        {
            if (m_vertexAttributeBuffers.at(i) != VK_NULL_HANDLE)
            {
                vkDestroyBuffer(m_device->GetNativeHandle(), m_vertexAttributeBuffers.at(i), nullptr);
            }
            if (m_vertexAttributeMemories.at(i) != VK_NULL_HANDLE)
            {
                vkFreeMemory(m_device->GetNativeHandle(), m_vertexAttributeMemories.at(i), nullptr);
            }
        }
        /* allocate new buffers for each attribute */
        for (uint32_t i = 0; i < numAttributes; ++i)
        {
            EVertexAttributeIndex attributeIndex = static_cast<EVertexAttributeIndex>(i);
            auto attributeBufferCreateInfo = NRI_VK_STRUCT(VkBufferCreateInfo);
            attributeBufferCreateInfo.size = m_triangleVertexCollection.GetAttributes(attributeIndex).GetBytes().size();
            attributeBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            attributeBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            NRI_VK_CHECK_RESULT(vkCreateBuffer(m_device->GetNativeHandle(), &attributeBufferCreateInfo, nullptr, &m_vertexAttributeBuffers.at(i)),
                                "Failed to create vertex attribute buffer for index {}",
                                i);

            /* query amount of memory required for this buffer to be allocated */
            VkMemoryRequirements attribBufferMemReqs;
            vkGetBufferMemoryRequirements(m_device->GetNativeHandle(), m_vertexAttributeBuffers.at(i), &attribBufferMemReqs);

            static auto GetRequiredMemoryTypeIndex = 
                [](const VkPhysicalDeviceMemoryProperties& memoryProperties, uint32_t typeFilter, VkMemoryPropertyFlags properties) -> uint32_t {
                    for (uint32_t memoryTypeIdx = 0; memoryTypeIdx < memoryProperties.memoryTypeCount; ++memoryTypeIdx)
                    {
                        if ((typeFilter & (1 << memoryTypeIdx)) && 
                            (memoryProperties.memoryTypes[memoryTypeIdx].propertyFlags & properties) == properties)
                        {
                            return memoryTypeIdx;
                        }
                    }
                    return UINT32_MAX;
                };
            const uint32_t memoryTypeIndex = GetRequiredMemoryTypeIndex(m_device->GetPhysicalDevice()->GetMemoryProperties(),
                                                                        attribBufferMemReqs.memoryTypeBits,
                                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            WARP_ASSERT(memoryTypeIndex != UINT32_MAX, "Failed to find suitable memory type index");

            auto attributeMemoryAllocateInfo = NRI_VK_STRUCT(VkMemoryAllocateInfo);
            attributeMemoryAllocateInfo.allocationSize = attribBufferMemReqs.size;
            attributeMemoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;
            NRI_VK_CHECK_RESULT(vkAllocateMemory(m_device->GetNativeHandle(), &attributeMemoryAllocateInfo, nullptr, &m_vertexAttributeMemories.at(i)),
                                "Failed to allocate device memory for vertex attribute buffer");

            auto bindInfo = NRI_VK_STRUCT(VkBindBufferMemoryInfo);
            bindInfo.buffer = m_vertexAttributeBuffers.at(i);
            bindInfo.memory = m_vertexAttributeMemories.at(i);
            bindInfo.memoryOffset = 0;
            NRI_VK_CHECK_RESULT(vkBindBufferMemory2(m_device->GetNativeHandle(), 1, &bindInfo), "Failed to bind vertex attribute buffer memory");

            // Populate vertex buffer with data from vertex collection
            void* pMappedData{};
            auto memoryMapInfo = NRI_VK_STRUCT(VkMemoryMapInfo);
            memoryMapInfo.flags = 0; // see VkMemoryMapFlagBits for more info
            memoryMapInfo.memory = m_vertexAttributeMemories.at(i);
            memoryMapInfo.offset = 0;
            memoryMapInfo.size = VK_WHOLE_SIZE;
            NRI_VK_CHECK_RESULT(vkMapMemory2(m_device->GetNativeHandle(), &memoryMapInfo, &pMappedData), "Failed to map vertex attribute buffer memory");
            {
                std::span attributeData = m_triangleVertexCollection.GetAttributes(attributeIndex).GetBytes();
                std::memcpy(pMappedData, attributeData.data(), attributeData.size());
            }
            auto memoryUnmapInfo = NRI_VK_STRUCT(VkMemoryUnmapInfo);
            memoryUnmapInfo.flags = 0;
            memoryUnmapInfo.memory = m_vertexAttributeMemories.at(i);
            NRI_VK_CHECK_RESULT(vkUnmapMemory2(m_device->GetNativeHandle(), &memoryUnmapInfo), "Failed to unmap vertex attribute buffer memory");

            // we don't need to flush memory from host to device after unmapping because we specified VK_MEMORY_PROPERTY_HOST_COHERENT_BIT memory.
            // alternatively vkFlushMappedMemoryRanges/vkInvalidateMappedMemoryRanges could be called
        }

        
    }

} // Warp::nri namespace