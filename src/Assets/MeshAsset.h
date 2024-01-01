#pragma once

#include <array>
#include <DirectXMesh.h>

#include "Asset.h"
#include "../Renderer/RHI/Resource.h"
#include "../Renderer/Vertex.h"
#include "../Math/Math.h"

namespace Warp
{

	// TODO: Rewrite this entirely. This is a bad structure as we need to somehow know which asset manager 
	class AssetManager;

	struct MeshAssetMaterial
	{
		bool HasBaseColor() const { return BaseColorProxy.IsValid(); }
		bool HasNormalMap() const { return NormalMapProxy.IsValid(); }
		bool HasMetalnessRoughnessMap() const { return MetalnessRoughnessMapProxy.IsValid(); }

		AssetManager* Manager = nullptr;
		AssetProxy BaseColorProxy;
		AssetProxy NormalMapProxy;
		AssetProxy MetalnessRoughnessMapProxy;

		Math::Vector4 BaseColor;
		Math::Vector2 MetalnessRoughness;
	};

	struct MeshAsset : public Asset
	{
		static constexpr EAssetType StaticType = EAssetType::Mesh;

		MeshAsset() : Asset(StaticType) {}

		uint32_t GetNumMeshlets() const { return static_cast<uint32_t>(Meshlets.size()); }
		uint32_t GetNumVertices() const { return NumVertices; }
		bool HasAttributes(size_t index) const { return !Attributes[index].empty(); }

		std::string Name;
		uint32_t NumVertices = 0;

		template<typename T>
		using AttributeArray = std::array<T, EVertexAttributes_NumAttributes>;

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

		MeshAssetMaterial Material;
	};

}