#pragma once

#include "common/memory/arc.h"
#include "common/enum_utils.h"

#include "platform_window.h"

#include "vulkan/nri_vk_common.h"
#include "vulkan/nri_vk_device.h"
#include "vulkan/nri_vk_instance.h"
#include "vulkan/nri_vk_surface.h"
#include "vulkan/nri_vk_swapchain.h"

// From Vulkan SDK if properly installed
#include <glm/glm.hpp>

#include <array>
#include <memory>
#include <span>

namespace Warp::nri
{

    class VertexAttributeArray
    {
    public:
        VertexAttributeArray() = default;
        VertexAttributeArray(uint32_t numElements, uint32_t stride, VkFormat format)
            : m_numElements(numElements)
            , m_stride(stride)
            , m_format(format)
            , m_bytes(std::make_unique<std::byte[]>(numElements * stride))
        {
            WARP_ASSERT(numElements > 0 && stride != 0, "Invalid vertex attribute array parameters");
        }

        inline constexpr uint32_t GetNumElements() const noexcept { return m_numElements; }
        inline constexpr uint32_t GetStride() const noexcept { return m_stride; }
        inline constexpr VkFormat GetFormat() const noexcept { return m_format; }
        
        inline constexpr std::span<const std::byte> GetBytes() const noexcept { return std::span(m_bytes.get(), m_numElements * m_stride); }
        inline constexpr std::span<std::byte> GetBytes() noexcept { return std::span(m_bytes.get(), m_numElements * m_stride); }

        template<typename T>
        inline constexpr std::span<const T> GetValues() const noexcept
        {
            WARP_ASSERT(sizeof(T) == m_stride, "Stride mismatch between T and m_stride is not allowed in vertex attribute array");
            return std::span(reinterpret_cast<const T*>(m_bytes.get()), m_numElements);
        }
        
        template<typename T>
        inline constexpr std::span<T> GetValues() noexcept
        {
            WARP_ASSERT(sizeof(T) == m_stride, "Stride mismatch between T and m_stride is not allowed in vertex attribute array");
            return std::span(reinterpret_cast<T*>(m_bytes.get()), m_numElements);
        }

    private:
        uint32_t m_numElements = UINT32_MAX;
        uint32_t m_stride = UINT32_MAX;
        VkFormat m_format = VK_FORMAT_UNDEFINED;
        std::unique_ptr<std::byte[]> m_bytes;
    }; // VertexAttributeArray struct

    enum class EVertexAttributeIndex : uint32_t
    {
        Position = 0,
        Color = 1,
        // Mark last enum value
        Last, 
    }; // EVertexAttribute enum class 

    struct VertexCollection
    {
        VertexCollection()
        {
            // TODO: Remove this hard-code when full models are supported. This is here just to render a triangle
            GetAttributes(EVertexAttributeIndex::Position) = VertexAttributeArray(3, sizeof(glm::vec2), VK_FORMAT_R32G32_SFLOAT);
            std::span positions = GetAttributes(EVertexAttributeIndex::Position).GetValues<glm::vec2>();
            positions[0] = glm::vec2(0.0f, -0.5f);
            positions[1] = glm::vec2(-0.5f, 0.5f);
            positions[2] = glm::vec2(0.5f, 0.5f);

            GetAttributes(EVertexAttributeIndex::Color) = VertexAttributeArray(3, sizeof(glm::vec3), VK_FORMAT_R32G32B32_SFLOAT);
            std::span colors = GetAttributes(EVertexAttributeIndex::Color).GetValues<glm::vec3>();
            colors[0] = glm::vec3(1.0f, 1.0f, 0.0f);
            colors[1] = glm::vec3(1.0f, 1.0f, 0.0f);
            colors[2] = glm::vec3(1.0f, 1.0f, 1.0f);
        }

        inline VkVertexInputBindingDescription GetBindingDescription(EVertexAttributeIndex attributeIndex, bool isPerVertex) const noexcept 
        { 
            const uint32_t idx = EnumValue(attributeIndex);
            return VkVertexInputBindingDescription {
                .binding = idx,
                .stride = vertexAttributes.at(idx).GetStride(),
                .inputRate = isPerVertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE
            };
        }

        inline VkVertexInputAttributeDescription GetAttributeDescription(EVertexAttributeIndex attributeIndex, uint32_t location) const noexcept
        {
            const uint32_t idx = EnumValue(attributeIndex);
            WARP_ASSERT(vertexAttributes.at(idx).GetNumElements() != UINT32_MAX, "Invalid number of elements for attribute index {}", idx);
            WARP_ASSERT(vertexAttributes.at(idx).GetStride() != UINT32_MAX, "Invalid stride for attribute index {}", idx);
            WARP_ASSERT(vertexAttributes.at(idx).GetFormat() != VK_FORMAT_UNDEFINED, "Invalid Vulkan format for attribute index {}", idx);
            return VkVertexInputAttributeDescription {
                .location = location,
                .binding = idx,
                .format = vertexAttributes.at(idx).GetFormat(),
                .offset = 0, // SoA do not require offsets
            };
        }

        const VertexAttributeArray& GetAttributes(EVertexAttributeIndex attributeIndex) const noexcept { return this->vertexAttributes.at(EnumValue(attributeIndex)); } 
        VertexAttributeArray& GetAttributes(EVertexAttributeIndex attributeIndex) noexcept { return this->vertexAttributes.at(EnumValue(attributeIndex)); } 

        std::array<VertexAttributeArray, EnumValue(EVertexAttributeIndex::Last)> vertexAttributes;
    }; // VertexCollection struct

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

        // does not require any additional initialization
        VertexCollection m_triangleVertexCollection;

        void InitTriangleVertexBuffers();
        std::array<VkBuffer, EnumValue(EVertexAttributeIndex::Last)> m_vertexAttributeBuffers;
        std::array<VkDeviceMemory, EnumValue(EVertexAttributeIndex::Last)> m_vertexAttributeMemories;
    };

} // Warp::nri namespace