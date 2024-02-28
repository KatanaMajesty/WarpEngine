#pragma once

#include <array>
#include <DirectXMesh.h>

#include "Asset.h"
#include "../Renderer/RHI/Resource.h"
#include "../Renderer/Vertex.h"

namespace Warp
{

	struct Submesh
	{
		uint32_t GetNumMeshlets() const { return static_cast<uint32_t>(Meshlets.size()); }
		uint32_t GetNumVertices() const { return NumVertices; }
		bool HasAttributes(size_t index) const { return !Attributes[index].empty(); }

		uint32_t NumVertices = 0;

		template<typename T>
		using AttributeArray = std::array<T, eVertexAttribute_NumAttributes>;

		// SoA representation of mesh vertices
		AttributeArray<std::vector<std::byte>> Attributes;
		AttributeArray<uint32_t>  AttributeStrides{};
		AttributeArray<RHIBuffer> Resources;

		std::vector<DirectX::Meshlet> Meshlets;
		std::vector<uint8_t> UniqueVertexIndices;
		std::vector<DirectX::MeshletTriangle> PrimitiveIndices;

		RHIBuffer MeshletBuffer;
		RHIBuffer UniqueVertexIndicesBuffer;
		RHIBuffer PrimitiveIndicesBuffer;
	};
	
	struct MeshAsset : Asset
	{
		static constexpr EAssetType StaticType = EAssetType::Mesh;

		MeshAsset(uint32_t ID) : Asset(ID, StaticType) {}

		uint32_t GetNumSubmeshes() const { return static_cast<uint32_t>(Submeshes.size()); }

		// It is safe to assume that Submeshes.size() == SubmeshMaterials.size();
		std::string Name;
		std::vector<Submesh> Submeshes;
		std::vector<AssetProxy> SubmeshMaterials;
	};

}