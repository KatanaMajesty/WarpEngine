#pragma once

#include <vector>

// Include this before DirectXMesh to use it with DirectX 12
#include "RHI/stdafx.h"
#include <DirectXMesh.h>

#include "RHI/Resource.h"
#include "../Math/Math.h"

namespace Warp
{

	using Meshlet = DirectX::Meshlet;
	using MeshletTriangle = DirectX::MeshletTriangle;

	enum EVertexAttributes
	{
		Positions = 0,
		Normals,
		TextureCoords,
		Tangents,
		Bitangents,
		NumAttributes,
	};

	struct StaticMesh
	{
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
		};

		VertexStream StreamOfVertices;

		std::vector<uint32_t> Indices;
		RHIBuffer IndexBuffer;
		uint32_t NumIndices;

		std::vector<Meshlet> Meshlets;
		std::vector<uint8_t> UniqueVertexIndices;
		std::vector<MeshletTriangle> PrimitiveIndices;

		RHIBuffer MeshletBuffer;
		RHIBuffer UniqueVertexIndicesBuffer;
		RHIBuffer PrimitiveIndicesBuffer;
	};

}