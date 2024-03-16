#include "MeshImporter.h"

#include <filesystem>

#include "../../Core/Application.h"

#include "TextureImporter.h"

// Include assets after application and renderer
#include "../MaterialAsset.h"
#include "../MeshAsset.h"
#include "../TextureAsset.h"

#include "../../Math/Math.h"
#include "../AssetManager.h"

namespace Warp
{

    AssetProxy MeshImporter::ImportStaticMeshFromFile(const std::string& filepath)
    {
        EAssetFormat format = GetFormat(std::filesystem::path(filepath).extension().string());
        if (format == EAssetFormat::Unknown)
        {
            WARP_LOG_ERROR("MeshImporter::ImportStaticMeshFromFile -> Unsupported mesh extension for {}", filepath);
            return AssetProxy();
        }

        AssetProxy proxy = AssetProxy();
        switch (format)
        {
        case EAssetFormat::Gltf:
            proxy = ImportStaticMeshFromGltfFile(filepath);
            break;
        default: WARP_ASSERT(false, "Shouldnt happen"); return AssetProxy();
        }

        if (!proxy.IsValid())
        {
            WARP_LOG_ERROR("MeshImporter::ImportStaticMeshFromFile -> Failed to import static mesh from \'{}\'", filepath);
            return AssetProxy();
        }

        MeshAsset* mesh = GetAssetManager()->GetAs<MeshAsset>(proxy);
        uint32_t numSubmeshes = mesh->GetNumSubmeshes();

        // Process every submesh
        for (uint32_t submeshIndex = 0; submeshIndex < numSubmeshes; ++submeshIndex)
        {
            // Load GPU resources
            Submesh& submesh = mesh->Submeshes[submeshIndex];

            // TODO: (13.02.2024) Using singleton? Umh...
            Renderer* renderer = Application::Get().GetRenderer();
            RHIDevice* Device = renderer->GetDevice();

            static constexpr uint32_t MeshletStride = sizeof(DirectX::Meshlet);
            static constexpr uint32_t UVIndexStride = sizeof(uint8_t);
            static constexpr uint32_t PrimitiveIndicesStride = sizeof(DirectX::MeshletTriangle);

            size_t meshletsInBytes = submesh.Meshlets.size() * MeshletStride;
            size_t uniqueVertexIndicesInBytes = submesh.UniqueVertexIndices.size() * UVIndexStride;
            size_t primitiveIndicesInBytes = submesh.PrimitiveIndices.size() * PrimitiveIndicesStride;

            // Sanity-check. If invalid submesh - continue
            if (meshletsInBytes == 0 || uniqueVertexIndicesInBytes == 0 || primitiveIndicesInBytes == 0)
            {
                WARP_LOG_WARN("MeshImporter::ImportStaticMeshFromFile -> Invalid meshlet data for \'{}\' mesh (submesh index {})", mesh->Name, submeshIndex);
                continue;
            }

            // Note of the FIX with "Wrong resource state"
            // The COPY flags (COPY_DEST and COPY_SOURCE) used as initial states represent states in the 3D/Compute type class. 
            // To use a resource initially on a Copy queue it should start in the COMMON state. 
            // The COMMON state can be used for all usages on a Copy queue using the implicit state transitions. 
            submesh.MeshletBuffer = RHIBuffer(Device,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_FLAG_NONE, meshletsInBytes);
            submesh.UniqueVertexIndicesBuffer = RHIBuffer(Device,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_FLAG_NONE, uniqueVertexIndicesInBytes);
            submesh.PrimitiveIndicesBuffer = RHIBuffer(Device,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_FLAG_NONE, primitiveIndicesInBytes);

            // Now perform resource copying from UPLOAD heap to DEFAULT heap (our mesh resource)
            RHICopyCommandContext& copyContext = renderer->GetCopyContext();
            copyContext.BeginCopy();
            copyContext.Open();
            {
                // NOTICE!
                // Specifically, a resource must be in the COMMON state before being used on DIRECT/COMPUTE (when previously used on COPY). 
                // This restriction doesn't exist when accessing data between DIRECT and COMPUTE queues.
                for (size_t i = 0; i < eVertexAttribute_NumAttributes; ++i)
                {
                    // Skip if no attribute
                    if (!submesh.HasAttributes(i))
                    {
                        continue;
                    }

                    uint32_t stride = submesh.AttributeStrides[i];
                    size_t sizeInBytes = submesh.GetNumVertices() * stride;
                    submesh.Resources[i] = RHIBuffer(Device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE, sizeInBytes);

                    copyContext.UploadToBuffer(&submesh.Resources[i], submesh.Attributes[i].data(), sizeInBytes);
                }

                copyContext.UploadToBuffer(&submesh.MeshletBuffer, submesh.Meshlets.data(), meshletsInBytes);
                copyContext.UploadToBuffer(&submesh.UniqueVertexIndicesBuffer, submesh.UniqueVertexIndices.data(), uniqueVertexIndicesInBytes);
                copyContext.UploadToBuffer(&submesh.PrimitiveIndicesBuffer, submesh.PrimitiveIndices.data(), primitiveIndicesInBytes);
            }
            copyContext.Close();

            UINT64 fenceValue = copyContext.Execute(false);
            copyContext.EndCopy(fenceValue);
        }

        return proxy;
    }

}