#include "MeshImporter.h"

#include <filesystem>

#include "../../Core/Application.h"

#include "TextureImporter.h"

// Include assets after application and renderer
#include "../MaterialAsset.h"
#include "../MeshAsset.h"
#include "../TextureAsset.h"

#include "../../Util/MeshLoader.h"
#include "../../Math/Math.h"
#include "../AssetManager.h"

namespace Warp
{

	AssetProxy MeshImporter::ImportStaticMeshFromFile(const std::string& filepath)
	{
		EAssetFormat format = GetFormat(std::filesystem::path(filepath).extension().string());
		if (format == EAssetFormat::Unknown)
		{
			WARP_LOG_ERROR("Unsupported mesh extension for {}", filepath);
			return AssetProxy();
		}

		AssetManager* manager = GetAssetManager();
		AssetProxy proxy = manager->GetAssetProxy(filepath);
		if (proxy.IsValid())
		{
			WARP_LOG_INFO("Returning cached asset proxy for static mesh at {}", filepath);
			return proxy;
		}

		MeshLoader::StaticMesh loadedMesh;
		switch (format)
		{
		case EAssetFormat::Gltf: 
			loadedMesh = std::move(MeshLoader::LoadStaticMeshFromGltfFile(filepath, MeshLoader::LoadDesc{ .GenerateTangents = true }));
			break;
		default: WARP_ASSERT(false, "Shouldnt happen"); return AssetProxy();
		}

		if (!loadedMesh.IsValid())
		{
			WARP_LOG_ERROR("Failed to load static mesh from {}", filepath);
			return AssetProxy();
		}

		proxy = manager->CreateAsset<MeshAsset>(filepath);
		if (!proxy.IsValid())
		{
			WARP_LOG_ERROR("MeshImporter::ImportStaticMeshFromFile -> Failed to create a MeshAsset for \'{}\'", filepath);
			return proxy;
		}

		MeshAsset* mesh = manager->GetAs<MeshAsset>(proxy);

		size_t numSubmeshes = loadedMesh.Submeshes.size();
		mesh->Name = loadedMesh.Name;
		mesh->Submeshes.resize(numSubmeshes);
		mesh->SubmeshMaterials.resize(numSubmeshes);
		WARP_ASSERT(numSubmeshes == loadedMesh.SubmeshMaterials.size()); // Sanity-check
		
		// https://github.com/microsoft/DirectXMesh/wiki/DirectXMesh
		// TODO: Add mesh optimization

		// Process every submesh
		for (size_t submeshIndex = 0; submeshIndex < numSubmeshes; ++submeshIndex)
		{
			Submesh& dstSubmesh = mesh->Submeshes[submeshIndex];
			MeshLoader::StaticMesh::Submesh& srcSubmesh = loadedMesh.Submeshes[submeshIndex];

			dstSubmesh.NumVertices = srcSubmesh.NumVertices;
			for (size_t attributeIndex = 0; attributeIndex < eVertexAttribute_NumAttributes; ++attributeIndex)
			{
				dstSubmesh.Attributes[attributeIndex] = std::move(srcSubmesh.Attributes[attributeIndex]);
				dstSubmesh.AttributeStrides[attributeIndex] = srcSubmesh.AttributeStrides[attributeIndex];
			}

			// Load, allocate and process default material
			AssetProxy& materialProxy = mesh->SubmeshMaterials[submeshIndex];
			MeshLoader::StaticMesh::SubmeshMaterial& srcMaterial = loadedMesh.SubmeshMaterials[submeshIndex];

			materialProxy = manager->CreateAsset<MaterialAsset>();
			MaterialAsset* dstMaterial = manager->GetAs<MaterialAsset>(materialProxy);

			dstMaterial->AlbedoMap = m_textureImporter.ImportFromMemory(srcMaterial.AlbedoMap);
			dstMaterial->NormalMap = m_textureImporter.ImportFromMemory(srcMaterial.NormalMap);
			dstMaterial->RoughnessMetalnessMap = m_textureImporter.ImportFromMemory(srcMaterial.RoughnessMetalnessMap);
			if (!dstMaterial->HasAlbedoMap())
				dstMaterial->Albedo = srcMaterial.AlbedoFactor;
			
			if (!dstMaterial->HasRoughnessMetalnessMap())
				dstMaterial->RoughnessMetalness = srcMaterial.RoughnessMetalnessFactor;

			if (!dstMaterial->HasNormalMap())
			{
				// Handle this some day? how to even handle this?
				WARP_LOG_WARN("No normal map! This will cease correct Pbr model calculations as we do not handle this!");
				WARP_ASSERT(false);
			}

			// Compute meshlets
			uint32_t numIndices = static_cast<uint32_t>(srcSubmesh.Indices.size());
			uint32_t* indices = srcSubmesh.Indices.data();

			Math::Vector3* meshPositions = reinterpret_cast<Math::Vector3*>(dstSubmesh.Attributes[eVertexAttribute_Positions].data());
			WARP_MAYBE_UNUSED HRESULT hr = DirectX::ComputeMeshlets(
				indices, numIndices / 3,
				meshPositions, dstSubmesh.GetNumVertices(),
				nullptr,
				dstSubmesh.Meshlets, dstSubmesh.UniqueVertexIndices, dstSubmesh.PrimitiveIndices);

			WARP_ASSERT(SUCCEEDED(hr), "Failed to compute meshlets for mesh");
			
			// Load GPU resources
			// TODO: (13.02.2024) Using singleton? Umh...
			Renderer* renderer = Application::Get().GetRenderer();
			RHIDevice* Device = renderer->GetDevice();

			static constexpr uint32_t MeshletStride = sizeof(DirectX::Meshlet);
			static constexpr uint32_t UVIndexStride = sizeof(uint8_t);
			static constexpr uint32_t PrimitiveIndicesStride = sizeof(DirectX::MeshletTriangle);

			size_t meshletsInBytes = dstSubmesh.Meshlets.size() * MeshletStride;
			size_t uniqueVertexIndicesInBytes = dstSubmesh.UniqueVertexIndices.size() * UVIndexStride;
			size_t primitiveIndicesInBytes = dstSubmesh.PrimitiveIndices.size() * PrimitiveIndicesStride;

			// Note of the FIX with "Wrong resource state"
			// The COPY flags (COPY_DEST and COPY_SOURCE) used as initial states represent states in the 3D/Compute type class. 
			// To use a resource initially on a Copy queue it should start in the COMMON state. 
			// The COMMON state can be used for all usages on a Copy queue using the implicit state transitions. 
			dstSubmesh.MeshletBuffer = RHIBuffer(Device,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_COMMON,
				D3D12_RESOURCE_FLAG_NONE, meshletsInBytes);
			dstSubmesh.UniqueVertexIndicesBuffer = RHIBuffer(Device,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_COMMON,
				D3D12_RESOURCE_FLAG_NONE, uniqueVertexIndicesInBytes);
			dstSubmesh.PrimitiveIndicesBuffer = RHIBuffer(Device,
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
					if (!dstSubmesh.HasAttributes(i))
					{
						continue;
					}

					uint32_t stride = dstSubmesh.AttributeStrides[i];
					size_t sizeInBytes = dstSubmesh.GetNumVertices() * stride;
					dstSubmesh.Resources[i] = RHIBuffer(Device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE, sizeInBytes);

					copyContext.UploadToBuffer(&dstSubmesh.Resources[i], dstSubmesh.Attributes[i].data(), sizeInBytes);
				}

				copyContext.UploadToBuffer(&dstSubmesh.MeshletBuffer, dstSubmesh.Meshlets.data(), meshletsInBytes);
				copyContext.UploadToBuffer(&dstSubmesh.UniqueVertexIndicesBuffer, dstSubmesh.UniqueVertexIndices.data(), uniqueVertexIndicesInBytes);
				copyContext.UploadToBuffer(&dstSubmesh.PrimitiveIndicesBuffer, dstSubmesh.PrimitiveIndices.data(), primitiveIndicesInBytes);
			}
			copyContext.Close();

			UINT64 fenceValue = copyContext.Execute(false);
			copyContext.EndCopy(fenceValue);
		}

		return proxy;
	}

}