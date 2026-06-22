#pragma once

#include "common/enum_utils.h"
#include "common/memory/arc.h"
#include "vulkan/nri_vk_buffer.h"
#include "vulkan/nri_vk_common.h"
#include "vulkan/nri_vk_device.h"
#include "vulkan/nri_vk_instance.h"
#include "vulkan/nri_vk_swapchain.h"

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <memory>
#include <span>

namespace Warp::nri
{

struct GlobalUniformBuffer
{
    glm::mat4 modelMatrix;
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
};

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

    inline constexpr std::span<const std::byte> GetBytes() const noexcept
    {
        return std::span(m_bytes.get(), m_numElements * m_stride);
    }
    inline constexpr std::span<std::byte> GetBytes() noexcept
    {
        return std::span(m_bytes.get(), m_numElements * m_stride);
    }

    template <typename T>
    inline constexpr std::span<const T> GetValues() const noexcept
    {
        WARP_ASSERT(sizeof(T) == m_stride,
                    "Stride mismatch between T and m_stride is not allowed in vertex attribute array");
        return std::span(reinterpret_cast<const T*>(m_bytes.get()), m_numElements);
    }

    template <typename T>
    inline constexpr std::span<T> GetValues() noexcept
    {
        WARP_ASSERT(sizeof(T) == m_stride,
                    "Stride mismatch between T and m_stride is not allowed in vertex attribute array");
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

// SoA for vertex attributes
struct VertexCollection
{
    VertexCollection()
    {
        GetAttributes(EVertexAttributeIndex::Position) =
            VertexAttributeArray(24, sizeof(glm::vec3), VK_FORMAT_R32G32B32_SFLOAT);
        std::span positions = GetAttributes(EVertexAttributeIndex::Position).GetValues<glm::vec3>();
        // +z
        positions[0] = glm::vec3(-0.5f, -0.5f, 0.5f);
        positions[1] = glm::vec3(0.5f, -0.5f, 0.5f);
        positions[2] = glm::vec3(0.5f, 0.5f, 0.5f);
        positions[3] = glm::vec3(-0.5f, 0.5f, 0.5f);
        // -z
        positions[4] = glm::vec3(0.5f, -0.5f, -0.5f);
        positions[5] = glm::vec3(-0.5f, -0.5f, -0.5f);
        positions[6] = glm::vec3(-0.5f, 0.5f, -0.5f);
        positions[7] = glm::vec3(0.5f, 0.5f, -0.5f);
        // -x
        positions[8] = glm::vec3(-0.5f, -0.5f, -0.5f);
        positions[9] = glm::vec3(-0.5f, -0.5f, 0.5f);
        positions[10] = glm::vec3(-0.5f, 0.5f, 0.5f);
        positions[11] = glm::vec3(-0.5f, 0.5f, -0.5f);
        // +x
        positions[12] = glm::vec3(0.5f, -0.5f, 0.5f);
        positions[13] = glm::vec3(0.5f, -0.5f, -0.5f);
        positions[14] = glm::vec3(0.5f, 0.5f, -0.5f);
        positions[15] = glm::vec3(0.5f, 0.5f, 0.5f);
        // +y
        positions[16] = glm::vec3(-0.5f, 0.5f, 0.5f);
        positions[17] = glm::vec3(0.5f, 0.5f, 0.5f);
        positions[18] = glm::vec3(0.5f, 0.5f, -0.5f);
        positions[19] = glm::vec3(-0.5f, 0.5f, -0.5f);
        // -y
        positions[20] = glm::vec3(-0.5f, -0.5f, -0.5f);
        positions[21] = glm::vec3(0.5f, -0.5f, -0.5f);
        positions[22] = glm::vec3(0.5f, -0.5f, 0.5f);
        positions[23] = glm::vec3(-0.5f, -0.5f, 0.5f);

        GetAttributes(EVertexAttributeIndex::Color) =
            VertexAttributeArray(24, sizeof(glm::vec3), VK_FORMAT_R32G32B32_SFLOAT);
        std::span colors = GetAttributes(EVertexAttributeIndex::Color).GetValues<glm::vec3>();
        // +z
        colors[0] = colors[1] = colors[2] = colors[3] = glm::vec3(1.0f, 0.0f, 0.0f);
        // -z
        colors[4] = colors[5] = colors[6] = colors[7] = glm::vec3(0.0f, 1.0f, 0.0f);
        // -x
        colors[8] = colors[9] = colors[10] = colors[11] = glm::vec3(0.0f, 0.0f, 1.0f);
        // +x
        colors[12] = colors[13] = colors[14] = colors[15] = glm::vec3(1.0f, 1.0f, 0.0f);
        // +y
        colors[16] = colors[17] = colors[18] = colors[19] = glm::vec3(1.0f, 0.0f, 1.0f);
        // -y
        colors[20] = colors[21] = colors[22] = colors[23] = glm::vec3(0.0f, 1.0f, 1.0f);

        indexAttribute = VertexAttributeArray(36, sizeof(uint16_t), VK_FORMAT_R16_UINT);
        std::span indices = indexAttribute.GetValues<uint16_t>();
        // Front
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        indices[3] = 2;
        indices[4] = 3;
        indices[5] = 0;
        // Back
        indices[6] = 4;
        indices[7] = 5;
        indices[8] = 6;
        indices[9] = 6;
        indices[10] = 7;
        indices[11] = 4;
        // Left
        indices[12] = 8;
        indices[13] = 9;
        indices[14] = 10;
        indices[15] = 10;
        indices[16] = 11;
        indices[17] = 8;
        // Right
        indices[18] = 12;
        indices[19] = 13;
        indices[20] = 14;
        indices[21] = 14;
        indices[22] = 15;
        indices[23] = 12;
        // Top
        indices[24] = 16;
        indices[25] = 17;
        indices[26] = 18;
        indices[27] = 18;
        indices[28] = 19;
        indices[29] = 16;
        // Bottom
        indices[30] = 20;
        indices[31] = 21;
        indices[32] = 22;
        indices[33] = 22;
        indices[34] = 23;
        indices[35] = 20;
    }

    inline VkVertexInputBindingDescription GetBindingDescription(EVertexAttributeIndex attributeIndex,
                                                                 bool isPerVertex) const noexcept
    {
        const uint32_t idx = EnumValue(attributeIndex);
        return VkVertexInputBindingDescription{.binding = idx,
                                               .stride = vertexAttributes.at(idx).GetStride(),
                                               .inputRate = isPerVertex ? VK_VERTEX_INPUT_RATE_VERTEX
                                                                        : VK_VERTEX_INPUT_RATE_INSTANCE};
    }

    inline VkVertexInputAttributeDescription GetAttributeDescription(EVertexAttributeIndex attributeIndex,
                                                                     uint32_t location) const noexcept
    {
        const uint32_t idx = EnumValue(attributeIndex);
        WARP_ASSERT(vertexAttributes.at(idx).GetNumElements() != UINT32_MAX,
                    "Invalid number of elements for attribute index {}", idx);
        WARP_ASSERT(vertexAttributes.at(idx).GetStride() != UINT32_MAX, "Invalid stride for attribute index {}", idx);
        WARP_ASSERT(vertexAttributes.at(idx).GetFormat() != VK_FORMAT_UNDEFINED,
                    "Invalid Vulkan format for attribute index {}", idx);
        return VkVertexInputAttributeDescription{
            .location = location,
            .binding = idx,
            .format = vertexAttributes.at(idx).GetFormat(),
            .offset = 0, // SoA do not require offsets
        };
    }

    const VertexAttributeArray& GetAttributes(EVertexAttributeIndex attributeIndex) const noexcept
    {
        return this->vertexAttributes.at(EnumValue(attributeIndex));
    }
    VertexAttributeArray& GetAttributes(EVertexAttributeIndex attributeIndex) noexcept
    {
        return this->vertexAttributes.at(EnumValue(attributeIndex));
    }

    std::array<VertexAttributeArray, EnumValue(EVertexAttributeIndex::Last)> vertexAttributes;

    // TODO: Rename vertex attribute array to something more generic when more attributes are supported, like instance
    // data for example. This is just a placeholder for now to render a triangle
    VertexAttributeArray indexAttribute;
}; // VertexCollection struct

struct RendererInfo
{
    SDL_Window* window = nullptr;
    /// @brief An amount of frames to be submitted inflight.
    /// For X frames in flight X frames will be submitted by the CPU to GPU before waiting for first submitted frame to
    /// finish
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
        // vkAcquireNextImageKHR(m_device->GetNativeHandle(), m_swapchain->GetNativeHandle(), UINT64_MAX, )
    }

    void Resize();

    // TODO: replace this with NextFrame?
    void RenderFrame();
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t swapchainImageIndex);
    void UpdateGlobalUniformBuffer(uint32_t swapchainImageIndex);

private:
    RendererInfo m_info;
    uint32_t m_frameIndex = 0;
    std::chrono::high_resolution_clock::time_point m_frameTimestamp = std::chrono::high_resolution_clock::now();

    void InitInstance();
    void InitDevice();
    void InitSwapchain();
    Arc<vk::NriInstance> m_instance;
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
    VertexCollection m_quadVertexCollection;

    void InitQuadVertexBuffers();
    std::array<Arc<vk::NriBuffer>, EnumValue(EVertexAttributeIndex::Last)> m_vertexAttributeBuffers;
    void InitQuadIndexBuffer();
    Arc<vk::NriBuffer> m_indexBuffer;

    void InitDescriptorPool();
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    void InitDescriptorSets();
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;

    void InitUniformBuffers();
    std::vector<Arc<vk::NriBuffer>> m_globalUniformBuffers;
};

} // namespace Warp::nri