#pragma once

#include <array>
#include <string>
#include <vector>

#include "ImageLoader.h"
#include "../../Math/Math.h"
#include "../../Renderer/Vertex.h"

namespace Warp::MeshLoader
{

	// mesh material
	struct MeshMaterial
	{
		ImageLoader::Image BaseColor;
		ImageLoader::Image NormalMap;
		ImageLoader::Image MetalnessRoughnessMap;

		Math::Vector4 BaseColorFactor;
		Math::Vector2 MetalnessRoughnessFactor;
	};

	// mesh
	// WARP Remarks: (Maybe do not?) We convert gltf's Vec4 tangents to Vec3 tangents/bitangents here
	// WARP Remarks: (TODO) We currently only do 32-bit indices and if they were 16-bit initially we just convert them
	struct Mesh
	{
		std::string Name;
		
		template<typename T>
		using AttributeArray = std::array<T, EVertexAttributes_NumAttributes>;

		uint32_t NumVertices;
		AttributeArray<std::vector<std::byte>> Attributes;
		AttributeArray<uint32_t> AttributeStrides;

		// TODO: We currently only do 32-bit indices and if they were 16-bit initially we just convert them
		// This should be changed and 16-bit indices should be supported
		std::vector<uint32_t> Indices;

		MeshMaterial Material;
	};

	struct LoadDesc
	{
		// Determines whether or not to generate tangents and bitangents (binormals)
		bool GenerateTangents = false;
	};

	
	std::vector<Mesh> LoadFromGltfFile(std::string_view filepath, const LoadDesc& desc);

}