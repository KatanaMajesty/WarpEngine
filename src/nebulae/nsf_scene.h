#pragma once

#include "vulkan/nri_vk_buffer.h"
#include "vulkan/nri_vk_device.h"

#include "common/memory/arc.h"
#include "common/enum_utils.h"

#include <filesystem>
#include <vector>
#include <string>
#include <array>
#include <memory>

// Nebulae scene format

namespace Warp
{

    enum class ENsfResult
    {
        Ok,
        Error,
    };

    enum class ENsfVertexAttribute
    {
        Position,
        Color,
        NumVertexAttributes,
    };
    static constexpr uint32_t kNsfVertexAttributeCount = EnumValue(ENsfVertexAttribute::NumVertexAttributes); 
    struct NsfSubmeshDeviceInfo
    {
        std::array<Arc<nri::vk::NriBuffer>, kNsfVertexAttributeCount> attributeBuffers;
        // std::array<uint32_t, kNsfVertexAttributeCount> attributeOffsetArray; // offsets in bytes

        Arc<nri::vk::NriBuffer> indexBuffer;
        // uint32_t indexOffset = UINT32_MAX;
        // VkFormat indexFormat = VK_FORMAT_UNDEFINED; // u16 or u32 is only supported in NSF
    };
    struct NsfSubmesh
    {
        uint32_t numVertices = 0;
        std::array<std::unique_ptr<std::byte[]>, kNsfVertexAttributeCount> attributeDataArray;
        std::array<uint32_t, kNsfVertexAttributeCount> attributeStrideArray;
        std::array<uint32_t, kNsfVertexAttributeCount> attributeOffsetArray;

        uint32_t numIndices = 0;
        std::unique_ptr<std::byte[]> indexData;
        uint32_t indexStride = UINT32_MAX;
        uint32_t indexOffset = UINT32_MAX;
    };
    struct NsfMesh
    {
        std::unique_ptr<NsfSubmesh[]> submeshes;
        std::unique_ptr<NsfSubmeshDeviceInfo[]> submeshDeviceInfos;
    };
    struct NsfMeshParseInfo
    {
        /// @brief If NriDevice is not nullptr, the mesh runtime rendering information will be uploaded to this device
        Arc<nri::vk::NriDevice> uploadDevice;
    };
    ENsfResult nsfGetCubeMesh(NsfMesh* mesh, const NsfMeshParseInfo& parseInfo) noexcept;
} // Warp namespace