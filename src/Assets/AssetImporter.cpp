#include "AssetImporter.h"

#include <array>
#include <filesystem>

#include "GltfLoader.h"
#include "../Core/Logger.h"
#include "../Core/Assert.h"
#include "../Renderer/Renderer.h"
#include "../Renderer/RHI/CommandContext.h"

namespace Warp
{

    std::vector<AssetProxy> MeshImporter::ImportFromFile(std::string_view filepath)
    {
		AssetFileExtension extension = GetFilepathExtension(filepath);
		if (extension == AssetFileExtension::Unknown)
		{
			WARP_LOG_ERROR("Unsupported mesh extension for {}", filepath);
			return {};
		}

		AssetManager* manager = GetAssetManager();
		std::vector<AssetProxy> meshes;

		switch (extension)
		{
		case AssetFileExtension::Gltf: meshes = GltfLoader().LoadFromFile(filepath, manager); break;
		default: WARP_ASSERT(false, "Shouldnt happen"); return {};
		}

		if (meshes.empty())
		{
			WARP_LOG_ERROR("Failed to load meshes from {}", filepath);
			return {};
		}

        // https://github.com/microsoft/DirectXMesh/wiki/DirectXMesh
        // TODO: Add mesh optimization

        // Compute meshlets and upload gpu resources
        for (AssetProxy proxy : meshes)
        {
			auto mesh = manager->GetAs<MeshAsset>(proxy);
            Math::Vector3* meshPositions = reinterpret_cast<Math::Vector3*>(mesh->StreamOfVertices.Attributes[EVertexAttributes::Positions].data());
            WARP_MAYBE_UNUSED HRESULT hr = DirectX::ComputeMeshlets(
                mesh->Indices.data(), mesh->GetNumIndices() / 3,
                meshPositions, mesh->StreamOfVertices.NumVertices,
                nullptr,
                mesh->Meshlets, mesh->UniqueVertexIndices, mesh->PrimitiveIndices);

            WARP_ASSERT(SUCCEEDED(hr), "Failed to compute meshlets for mesh");
			UploadGpuResources(mesh);
        }
        
		return meshes;
    }

	void MeshImporter::UploadGpuResources(MeshAsset* mesh)
	{
		// TODO: REWRITE RHI RESOURCE HANDLING;
		Renderer* renderer = GetRenderer();
		RHIDevice* Device = renderer->GetDevice();

		// Fill upload resources with data
		static constexpr uint32_t IndexStride = sizeof(uint32_t);
		const size_t indicesInBytes = mesh->GetNumIndices() * IndexStride;
		RHIBuffer uploadIndexBuffer(Device, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE, IndexStride, indicesInBytes);

		std::memcpy(uploadIndexBuffer.GetCpuVirtualAddress<std::byte>(), mesh->Indices.data(), indicesInBytes);

		// Asset that we have indices
		mesh->IndexBuffer = RHIBuffer(Device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE, IndexStride, indicesInBytes);

		StaticMesh::VertexStream::AttributeArray<RHIBuffer> uploadResources;
		for (size_t i = 0; i < EVertexAttributes::NumAttributes; ++i)
		{
			// Skip if no attribute
			if (!mesh->HasAttributes(i))
			{
				continue;
			}

			uint32_t stride = mesh->StreamOfVertices.AttributeStrides[i];
			size_t sizeInBytes = mesh->StreamOfVertices.NumVertices * stride;
			uploadResources[i] = RHIBuffer(Device, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE, stride, sizeInBytes);

			std::memcpy(uploadResources[i].GetCpuVirtualAddress<std::byte>(), mesh->StreamOfVertices.Attributes[i].data(), sizeInBytes);

			mesh->StreamOfVertices.Resources[i] = RHIBuffer(Device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE, stride, sizeInBytes);
		}

		static constexpr uint32_t MeshletStride = sizeof(Meshlet);
		static constexpr uint32_t UVIndexStride = sizeof(uint8_t);
		static constexpr uint32_t PrimitiveIndicesStride = sizeof(MeshletTriangle);

		size_t meshletsInBytes = mesh->Meshlets.size() * sizeof(Meshlet);
		size_t uniqueVertexIndicesInBytes = mesh->UniqueVertexIndices.size() * UVIndexStride;
		size_t primitiveIndicesInBytes = mesh->PrimitiveIndices.size() * PrimitiveIndicesStride;

		RHIBuffer uploadMeshletBuffer = RHIBuffer(Device,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE, MeshletStride, meshletsInBytes);
		RHIBuffer uploadUniqueVertexIndicesBuffer = RHIBuffer(Device,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE, UVIndexStride, uniqueVertexIndicesInBytes);
		RHIBuffer uploadPrimitiveIndicesBuffer = RHIBuffer(Device,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE, PrimitiveIndicesStride, primitiveIndicesInBytes);

		std::memcpy(uploadMeshletBuffer.GetCpuVirtualAddress<std::byte>(), mesh->Meshlets.data(), meshletsInBytes);
		std::memcpy(uploadUniqueVertexIndicesBuffer.GetCpuVirtualAddress<std::byte>(), mesh->UniqueVertexIndices.data(), uniqueVertexIndicesInBytes);
		std::memcpy(uploadPrimitiveIndicesBuffer.GetCpuVirtualAddress<std::byte>(), mesh->PrimitiveIndices.data(), primitiveIndicesInBytes);

		// Note of the FIX with "Wrong resource state"
		// The COPY flags (COPY_DEST and COPY_SOURCE) used as initial states represent states in the 3D/Compute type class. 
		// To use a resource initially on a Copy queue it should start in the COMMON state. 
		// The COMMON state can be used for all usages on a Copy queue using the implicit state transitions. 
		mesh->MeshletBuffer = RHIBuffer(Device,
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_FLAG_NONE, MeshletStride, meshletsInBytes);
		mesh->UniqueVertexIndicesBuffer = RHIBuffer(Device,
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_FLAG_NONE, UVIndexStride, uniqueVertexIndicesInBytes);
		mesh->PrimitiveIndicesBuffer = RHIBuffer(Device,
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_FLAG_NONE, PrimitiveIndicesStride, primitiveIndicesInBytes);

		// Now perform resource copying from UPLOAD heap to DEFAULT heap (our mesh resource)
		RHICommandContext& copyContext = renderer->GetCopyContext();
		copyContext.Open();
		{
			// NOTICE!
			// Specifically, a resource must be in the COMMON state before being used on DIRECT/COMPUTE (when previously used on COPY). 
			// This restriction doesn't exist when accessing data between DIRECT and COMPUTE queues.
			copyContext.CopyResource(&mesh->IndexBuffer, &uploadIndexBuffer);
			for (size_t i = 0; i < EVertexAttributes::NumAttributes; ++i)
			{
				// Skip if no attribute
				if (!mesh->HasAttributes(i))
				{
					continue;
				}

				copyContext.CopyResource(&mesh->StreamOfVertices.Resources[i], &uploadResources[i]);
			}

			copyContext.CopyResource(&mesh->MeshletBuffer, &uploadMeshletBuffer);
			copyContext.CopyResource(&mesh->UniqueVertexIndicesBuffer, &uploadUniqueVertexIndicesBuffer);
			copyContext.CopyResource(&mesh->PrimitiveIndicesBuffer, &uploadPrimitiveIndicesBuffer);
		}
		copyContext.Close();
		copyContext.Execute(true); // TODO: We wait for completion, though shouldnt
	}

}