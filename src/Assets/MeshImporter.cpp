#include "MeshImporter.h"

#include "../Core/Application.h"
#include "TextureImporter.h"
#include "TextureAsset.h"
#include "Util/MeshLoader.h"
#include "../Math/Math.h"

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

		std::vector<MeshLoader::Mesh> loadedMeshes;
		switch (extension)
		{
		case AssetFileExtension::Gltf: 
			loadedMeshes = MeshLoader::LoadFromGltfFile(filepath, MeshLoader::LoadDesc{ .GenerateTangents = true });
			break;
		default: WARP_ASSERT(false, "Shouldnt happen"); return {};
		}

		if (loadedMeshes.empty())
		{
			WARP_LOG_ERROR("Failed to load meshes from {}", filepath);
			return {};
		}

		AssetManager* manager = GetAssetManager();

		std::vector<AssetProxy> meshes;
		meshes.reserve(loadedMeshes.size());
		
		// https://github.com/microsoft/DirectXMesh/wiki/DirectXMesh
		// TODO: Add mesh optimization

		// Compute meshlets, materials and upload gpu resources
		for (MeshLoader::Mesh& loadedMesh : loadedMeshes)
		{
			// Create mesh asset for each loaded mesh, move the buffers
			AssetProxy proxy = manager->CreateAsset<MeshAsset>();
			meshes.push_back(proxy);

			auto mesh = manager->GetAs<MeshAsset>(proxy);
			mesh->Name = std::move(loadedMesh.Name);
			mesh->NumVertices = loadedMesh.NumVertices;

			for (size_t i = 0; i < EVertexAttributes_NumAttributes; ++i)
			{
				mesh->Attributes[i] = std::move(loadedMesh.Attributes[i]);
				mesh->AttributeStrides[i] = std::move(loadedMesh.AttributeStrides[i]);
			}

			// Create assets for a material
			LoadMeshMaterial(mesh, loadedMesh.Material);

			// Compute meshlets
			uint32_t numIndices = static_cast<uint32_t>(loadedMesh.Indices.size());
			uint32_t* indices = loadedMesh.Indices.data();

			Math::Vector3* meshPositions = reinterpret_cast<Math::Vector3*>(mesh->Attributes[EVertexAttributes_Positions].data());
			WARP_MAYBE_UNUSED HRESULT hr = DirectX::ComputeMeshlets(
				indices, numIndices / 3,
				meshPositions, mesh->GetNumVertices(),
				nullptr,
				mesh->Meshlets, mesh->UniqueVertexIndices, mesh->PrimitiveIndices);

			WARP_ASSERT(SUCCEEDED(hr), "Failed to compute meshlets for mesh");
			LoadGpuResources(mesh);
		}

		return meshes;
	}

	void MeshImporter::LoadMeshMaterial(MeshAsset* mesh, const MeshLoader::MeshMaterial& loadedMaterial)
	{
		// TODO: We use Application's texture importer, which may use different asset manager? This is also a bad way to handle textures
		TextureImporter* textureImporter = Application::Get().GetTextureImporter();
		mesh->Material.Manager = textureImporter->GetAssetManager();

		if (loadedMaterial.BaseColor.IsValid())
		{
			mesh->Material.BaseColorProxy = textureImporter->ImportFromImage(loadedMaterial.BaseColor);
		}
		else
		{
			mesh->Material.BaseColor = loadedMaterial.BaseColorFactor;
		}

		if (loadedMaterial.NormalMap.IsValid())
		{
			mesh->Material.NormalMapProxy = textureImporter->ImportFromImage(loadedMaterial.NormalMap);
		}

		if (loadedMaterial.MetalnessRoughnessMap.IsValid())
		{
			mesh->Material.MetalnessRoughnessMapProxy = textureImporter->ImportFromImage(loadedMaterial.NormalMap);
		}
		else
		{
			mesh->Material.MetalnessRoughness = loadedMaterial.MetalnessRoughnessFactor;
		}
	}

	void MeshImporter::LoadGpuResources(MeshAsset* mesh)
	{
		Renderer* renderer = GetRenderer();
		RHIDevice* Device = renderer->GetDevice();

		static constexpr uint32_t MeshletStride = sizeof(DirectX::Meshlet);
		static constexpr uint32_t UVIndexStride = sizeof(uint8_t);
		static constexpr uint32_t PrimitiveIndicesStride = sizeof(DirectX::MeshletTriangle);

		size_t meshletsInBytes = mesh->Meshlets.size() * MeshletStride;
		size_t uniqueVertexIndicesInBytes = mesh->UniqueVertexIndices.size() * UVIndexStride;
		size_t primitiveIndicesInBytes = mesh->PrimitiveIndices.size() * PrimitiveIndicesStride;

		// Note of the FIX with "Wrong resource state"
		// The COPY flags (COPY_DEST and COPY_SOURCE) used as initial states represent states in the 3D/Compute type class. 
		// To use a resource initially on a Copy queue it should start in the COMMON state. 
		// The COMMON state can be used for all usages on a Copy queue using the implicit state transitions. 
		mesh->MeshletBuffer = RHIBuffer(Device,
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_FLAG_NONE, meshletsInBytes);
		mesh->UniqueVertexIndicesBuffer = RHIBuffer(Device,
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_FLAG_NONE, uniqueVertexIndicesInBytes);
		mesh->PrimitiveIndicesBuffer = RHIBuffer(Device,
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
			for (size_t i = 0; i < EVertexAttributes_NumAttributes; ++i)
			{
				// Skip if no attribute
				if (!mesh->HasAttributes(i))
				{
					continue;
				}

				uint32_t stride = mesh->AttributeStrides[i];
				size_t sizeInBytes = mesh->GetNumVertices() * stride;
				mesh->Resources[i] = RHIBuffer(Device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE, sizeInBytes);

				copyContext.UploadToBuffer(&mesh->Resources[i], mesh->Attributes[i].data(), sizeInBytes);
			}

			copyContext.UploadToBuffer(&mesh->MeshletBuffer, mesh->Meshlets.data(), meshletsInBytes);
			copyContext.UploadToBuffer(&mesh->UniqueVertexIndicesBuffer, mesh->UniqueVertexIndices.data(), uniqueVertexIndicesInBytes);
			copyContext.UploadToBuffer(&mesh->PrimitiveIndicesBuffer, mesh->PrimitiveIndices.data(), primitiveIndicesInBytes);
		}
		copyContext.Close();

		UINT64 fenceValue = copyContext.Execute(false);
		copyContext.EndCopy(fenceValue);
	}

}