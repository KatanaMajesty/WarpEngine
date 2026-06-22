#include "nsf_scene.h"

#include "common/assert.h"
#include "common/cross_log.h"

#include <span>

namespace Warp
{

ENsfResult nsfGetCubeMesh(NsfMesh* mesh, const NsfMeshParseInfo& parseInfo) noexcept
{
    static constexpr uint32_t CubeNumVertices = 24;
    static constexpr std::array<glm::vec3, CubeNumVertices> CubePositions = {
        // +z
        glm::vec3(-0.5f, -0.5f, 0.5f),
        glm::vec3(0.5f, -0.5f, 0.5f),
        glm::vec3(0.5f, 0.5f, 0.5f),
        glm::vec3(-0.5f, 0.5f, 0.5f),
        // -z
        glm::vec3(0.5f, -0.5f, -0.5f),
        glm::vec3(-0.5f, -0.5f, -0.5f),
        glm::vec3(-0.5f, 0.5f, -0.5f),
        glm::vec3(0.5f, 0.5f, -0.5f),
        // -x
        glm::vec3(-0.5f, -0.5f, -0.5f),
        glm::vec3(-0.5f, -0.5f, 0.5f),
        glm::vec3(-0.5f, 0.5f, 0.5f),
        glm::vec3(-0.5f, 0.5f, -0.5f),
        // +x
        glm::vec3(0.5f, -0.5f, 0.5f),
        glm::vec3(0.5f, -0.5f, -0.5f),
        glm::vec3(0.5f, 0.5f, -0.5f),
        glm::vec3(0.5f, 0.5f, 0.5f),
        // +y
        glm::vec3(-0.5f, 0.5f, 0.5f),
        glm::vec3(0.5f, 0.5f, 0.5f),
        glm::vec3(0.5f, 0.5f, -0.5f),
        glm::vec3(-0.5f, 0.5f, -0.5f),
        // -y
        glm::vec3(-0.5f, -0.5f, -0.5f),
        glm::vec3(0.5f, -0.5f, -0.5f),
        glm::vec3(0.5f, -0.5f, 0.5f),
        glm::vec3(-0.5f, -0.5f, 0.5f),
    };
    static constexpr std::array<glm::vec3, CubeNumVertices> CubeColors = {
        // +z
        glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        // -z
        glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        // -x
        glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        // +x
        glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 0.0f),
        // +y
        glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 1.0f),
        glm::vec3(1.0f, 0.0f, 1.0f),
        // -y
        glm::vec3(0.0f, 1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 1.0f, 1.0f)};

    static constexpr uint32_t CubeNumIndices = 36;
    static constexpr std::array<uint16_t, CubeNumIndices> CubeIndices = {// Front
                                                                         0, 1, 2, 2, 3, 0,
                                                                         // Back
                                                                         4, 5, 6, 6, 7, 4,
                                                                         // Left
                                                                         8, 9, 10, 10, 11, 8,
                                                                         // Right
                                                                         12, 13, 14, 14, 15, 12,
                                                                         // Top
                                                                         16, 17, 18, 18, 19, 16,
                                                                         // Bottom
                                                                         20, 21, 22, 22, 23, 20};

    WARP_ASSERT(mesh != nullptr);
    mesh->submeshes = std::make_unique<NsfSubmesh[]>(1);
    NsfSubmesh& submesh = mesh->submeshes[0];
    submesh.numVertices = CubeNumVertices;

    // Positions
    static constexpr uint32_t CubePositionsStride = sizeof(decltype(CubePositions)::value_type);
    static constexpr uint32_t CubePositionsSizeInBytes = CubePositionsStride * CubeNumVertices;
    std::unique_ptr<std::byte[]>& rawPositions =
        submesh.attributeDataArray.at(EnumValue(ENsfVertexAttribute::Position));
    rawPositions = std::make_unique<std::byte[]>(CubePositionsSizeInBytes);
    std::memcpy(rawPositions.get(), CubePositions.data(), CubePositionsSizeInBytes);
    submesh.attributeOffsetArray.at(EnumValue(ENsfVertexAttribute::Position)) = 0;
    submesh.attributeStrideArray.at(EnumValue(ENsfVertexAttribute::Position)) = CubePositionsStride;

    // Colors
    static constexpr size_t CubeColorsStride = sizeof(decltype(CubeColors)::value_type);
    static constexpr size_t CubeColorsSizeInBytes = CubeColorsStride * CubeNumVertices;
    std::unique_ptr<std::byte[]>& rawColors = submesh.attributeDataArray.at(EnumValue(ENsfVertexAttribute::Color));
    rawColors = std::make_unique<std::byte[]>(CubeColorsSizeInBytes);
    std::memcpy(rawColors.get(), CubeColors.data(), CubeColorsSizeInBytes);
    submesh.attributeOffsetArray.at(EnumValue(ENsfVertexAttribute::Color)) = 0;
    submesh.attributeStrideArray.at(EnumValue(ENsfVertexAttribute::Color)) = CubeColorsStride;

    // Indices
    submesh.numIndices = CubeNumIndices;
    static constexpr size_t CubeIndexStride = sizeof(decltype(CubeIndices)::value_type);
    static constexpr size_t CubeIndexSizeInBytes = CubeIndexStride * CubeNumIndices;
    submesh.indexData = std::make_unique<std::byte[]>(CubeColorsSizeInBytes);
    std::memcpy(submesh.indexData.get(), CubeIndices.data(), CubeIndexSizeInBytes);
    submesh.indexOffset = 0;
    submesh.indexStride = CubeColorsStride;

    if (parseInfo.uploadDevice)
    {
        // static constexpr VkFormat CubeIndexFormat =
        //     CubeIndexStride == sizeof(uint32_t) ? VK_FORMAT_R32_UINT : VK_FORMAT_R16_UINT;

        // ...
    }

    return ENsfResult::Ok;
}

} // namespace Warp
