#pragma once

#include <DirectXMesh.h>

#include "Asset.h"
#include "../Renderer/RHI/Resource.h"

namespace Warp
{

	enum EVertexAttributes
	{
		Positions = 0,
		Normals,
		TextureCoords,
		Tangents,
		Bitangents,
		NumAttributes,
	};

	struct MeshAsset : public Asset
	{
		static constexpr EAssetType StaticType = EAssetType::Mesh;

		MeshAsset() : Asset(StaticType) {}

		uint32_t GetNumIndices() const { return static_cast<uint32_t>(Indices.size()); }
		uint32_t GetNumVertices() const { return StreamOfVertices.NumVertices; }
		uint32_t GetNumMeshlets() const { return static_cast<uint32_t>(Meshlets.size()); }
		bool HasAttributes(size_t index) const { return !StreamOfVertices.Attributes[index].empty(); }

		std::string Name;

		// SoA representation of mesh vertices
		struct VertexStream
		{
			template<typename T>
			using AttributeArray = std::array<T, EVertexAttributes::NumAttributes>;

			AttributeArray<std::vector<std::byte>> Attributes;
			AttributeArray<uint32_t>  AttributeStrides;
			AttributeArray<RHIBuffer> Resources;

			uint32_t NumVertices = 0;
		} StreamOfVertices;

		// TODO: Maybe remove indices from mesh?
		std::vector<uint32_t> Indices;
		RHIBuffer IndexBuffer;

		std::vector<DirectX::Meshlet> Meshlets;
		std::vector<uint8_t> UniqueVertexIndices;
		std::vector<DirectX::MeshletTriangle> PrimitiveIndices;

		RHIBuffer MeshletBuffer;
		RHIBuffer UniqueVertexIndicesBuffer;
		RHIBuffer PrimitiveIndicesBuffer;
	};

}