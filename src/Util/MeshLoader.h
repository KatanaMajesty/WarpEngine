#pragma once

#include <array>
#include <string>
#include <vector>

#include "ImageLoader.h"
#include "../Math/Math.h"
#include "../Renderer/Vertex.h"

namespace Warp::MeshLoader
{

	// WARP Remarks: (Maybe do not?) We convert gltf's Vec4 tangents to Vec3 tangents/bitangents here
	// WARP Remarks: (TODO) We currently only do 32-bit indices and if they were 16-bit initially we just convert them
	struct StaticMesh
	{
		using ESubmeshProperties = uint16_t;
		enum  ESubmeshProperty
		{
			eSubmeshProperty_None = 0,
			eSubmeshProperty_HasMaterial = 1,
		};

		struct Submesh
		{
			template<typename T>
			using AttributeArray = std::array<T, eVertexAttribute_NumAttributes>;

			uint32_t NumVertices = 0;
			AttributeArray<std::vector<std::byte>> Attributes = {};
			AttributeArray<uint32_t> AttributeStrides = {};

			// TODO: We currently only do 32-bit indices and if they were 16-bit initially we just convert them
			// This should be changed and 16-bit indices should be supported
			std::vector<uint32_t> Indices;

			ESubmeshProperties Properties = eSubmeshProperty_None;
		};

		// Submesh material
		struct SubmeshMaterial
		{
			ImageLoader::Image AlbedoMap;
			ImageLoader::Image NormalMap;
			ImageLoader::Image RoughnessMetalnessMap;

			Math::Vector4 AlbedoFactor;
			Math::Vector2 RoughnessMetalnessFactor;
		};

		bool IsValid() const { return !Submeshes.empty() && Submeshes.size() == SubmeshMaterials.size(); }

		std::string Name;
		std::vector<Submesh> Submeshes;
		std::vector<SubmeshMaterial> SubmeshMaterials;
	};

	struct LoadDesc
	{
		// Determines whether or not to generate tangents and bitangents (binormals)
		bool GenerateTangents = false;
	};
	
	StaticMesh LoadStaticMeshFromGltfFile(std::string_view filepath, const LoadDesc& desc);

}