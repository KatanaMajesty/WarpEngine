#include "ModelLoader.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include <vector>
#include <cstring>

#include "../Core/Logger.h"
#include "../Core/Assert.h"

// TODO: Remove these
#include "../Core/Application.h"
#include "../Renderer/Renderer.h"

// TODO: Warp Asset Loader does not support sparse GLTF accessors currently. Will be removed asap
#define WAL_GLTF_SPARSE_NOIMPL(accessor) { if (accessor->is_sparse) { WARP_YIELD_NOIMPL(); } }

namespace Warp
{

	// Quickstart with glTF https://www.khronos.org/files/gltf20-reference-guide.pdf
	// Afterwards we chill here - https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html

	AssetProxy ModelLoader::Load(std::string_view filepath)
	{
		cgltf_data* data = nullptr;
		cgltf_options options = {};
		cgltf_result result = cgltf_parse_file(&options, filepath.data(), &data);
		if (result != cgltf_result_success)
		{
			WARP_LOG_ERROR("Failed to parse GLTF model at {}", filepath);
			return AssetProxy();
		}

		// Load bin buffers as well
		result = cgltf_load_buffers(&options, data, filepath.data());
		if (result != cgltf_result_success)
		{
			WARP_LOG_ERROR("Failed to parse buffers for GLTF model at {}", filepath);
			return AssetProxy();
		}

		AssetProxy proxy = m_assetManager->CreateAsset<ModelAsset>();

		ModelAsset* model = m_assetManager->GetAs<ModelAsset>(proxy);
		for (size_t i = 0; i < data->scenes_count; ++i)
		{
			cgltf_scene& scene = data->scenes[i];

			// We just process each node as if it is a static mesh node
			for (size_t nodeIndex = 0; nodeIndex < scene.nodes_count; ++nodeIndex)
			{
				cgltf_node* node = scene.nodes[nodeIndex];

				// Process mesh
				ProcessStaticMeshNode(model, node);
			}
		}
		
		// TODO: Check if succeeded. If not - destroy asset proxy

		cgltf_free(data);
		return proxy;
	}

	void ModelLoader::ProcessStaticMeshNode(ModelAsset* model, cgltf_node* node)
	{
		// TODO: Do we need this check? Remove it maybe
		if (!node || !model)
			return;

		// TODO: Add materials

		// TODO: We never use local to model transformation
		Math::Matrix LocalToModel = GetLocalToModel(node);

		cgltf_mesh* glTFMesh = node->mesh;
		for (size_t primitiveIndex = 0; primitiveIndex < glTFMesh->primitives_count; ++primitiveIndex)
		{
			// Create mesh for our model
			// TODO: Change convention from models/meshes to meshes/submeshes (?)
			StaticMesh& mesh = model->AddStaticMesh();
			mesh.Name = glTFMesh->name; // TODO: This name will be same for each mesh???

			cgltf_primitive& primitive = glTFMesh->primitives[primitiveIndex];
			cgltf_primitive_type type = primitive.type;

			// We only use triangles for now. Will be removed in future. This is just in case
			WARP_ASSERT(type == cgltf_primitive_type_triangles);

			ProcessStaticMeshAttributes(mesh, &primitive);

			ComputeMeshletsAndOptimize(mesh);
			UploadMeshResources(mesh);
		}

		// Process all its children
		for (size_t i = 0; i < node->children_count; ++i)
		{
			ProcessStaticMeshNode(model, node->children[i]);
		}
	}

	template<typename AttributeType, cgltf_component_type ReqType>
	void FillFromAccessor(std::vector<AttributeType>& dest, cgltf_accessor* accessor)
	{
		static constexpr size_t AttributeSize = sizeof(AttributeType);
		WARP_ASSERT(accessor->stride == AttributeSize && accessor->component_type == ReqType);

		cgltf_buffer_view* bufferView = accessor->buffer_view;

		// TODO: Check if this is reliable enough in all cases
		size_t sz = accessor->count * AttributeSize;
		const void* buf = bufferView->data ? bufferView->data : bufferView->buffer->data; WARP_ASSERT(buf);
		const void* src = static_cast<const std::byte*>(buf) + bufferView->offset;

		dest.clear();
		dest.resize(accessor->count);
		std::memcpy(dest.data(), src, sz);
	}

	template<typename AttributeType, cgltf_component_type ReqType>
	void FillBytesFromAccessor(std::vector<std::byte>& dest, uint32_t& stride, cgltf_accessor* accessor)
	{
		WARP_ASSERT(accessor->stride == sizeof(AttributeType), accessor->component_type == ReqType);
		cgltf_buffer_view* bufferView = accessor->buffer_view;

		// TODO: Check if this is reliable enough in all cases
		size_t sz = accessor->count * accessor->stride;
		const void* buf = bufferView->data ? bufferView->data : bufferView->buffer->data; WARP_ASSERT(buf);
		const void* src = static_cast<const std::byte*>(buf) + bufferView->offset;

		dest.clear();
		dest.resize(sz);
		std::memcpy(dest.data(), src, sz);

		// TODO: Safety checks?
		stride = static_cast<uint32_t>(accessor->stride);
	}

	void ModelLoader::ProcessStaticMeshAttributes(StaticMesh& mesh, cgltf_primitive* primitive)
	{
		cgltf_accessor* indices = primitive->indices;
		if (!indices)
		{
			// Indices property is not defined
			// Number of vertex indices to render is defined by the count property of attribute accessors (with the implied values from range [0..count))
			WARP_LOG_FATAL("no indices property! Mesh: {}", mesh.Name);
			WARP_YIELD_NOIMPL(); // We do not support non-indexed geometry currently
		}
		else
		{
			WAL_GLTF_SPARSE_NOIMPL(indices);
			if (indices->component_type == cgltf_component_type_r_16u)
			{
				// if u16, manually convert u16 to u32
				std::vector<uint16_t> indices16;
				FillFromAccessor<uint16_t, cgltf_component_type_r_16u>(indices16, indices);
				mesh.Indices.reserve(indices16.size());
				for (uint16_t i : indices16)
				{
					mesh.Indices.push_back(i);
				}

				mesh.NumIndices = static_cast<uint32_t>(indices->count);
			}
			else FillFromAccessor<uint32_t, cgltf_component_type_r_32u>(mesh.Indices, indices);
		}

		// From glTF 2.0 spec https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#geometry-overview
		// Tangent is stored in Vec4, where Vec4.w is a sign value indicating handedness of the tangent basis
		// We can use that information to calculate bitangent efficiently
		// We will manually convert signed tangents from glTF to vec3 tangents/bitangents
		std::vector<Math::Vector4> gltfTangents;

		// All attribute accessors for a given primitive MUST have the same count
		for (size_t attributeIndex = 0; attributeIndex < primitive->attributes_count; ++attributeIndex)
		{
			// Process attribute
			cgltf_attribute& attribute = primitive->attributes[attributeIndex];
			cgltf_accessor* accessor = attribute.data; WARP_ASSERT(accessor);
			WAL_GLTF_SPARSE_NOIMPL(accessor);
			switch (attribute.type)
			{
			case cgltf_attribute_type_position:
			{
				mesh.StreamOfVertices.NumVertices = static_cast<uint32_t>(accessor->count);

				FillBytesFromAccessor<Math::Vector3, cgltf_component_type_r_32f>(
					mesh.StreamOfVertices.Attributes[EVertexAttributes::Positions],
					mesh.StreamOfVertices.AttributeStrides[EVertexAttributes::Positions], accessor);

				// According to spec: POSITION accessor MUST have its min and max properties defined.
				WARP_ASSERT(accessor->has_max && accessor->has_min);

				// TODO: Calculate center of the mesh, calculate the length of the mesh for AABB
			};  break;
			case cgltf_attribute_type_normal: FillBytesFromAccessor<Math::Vector3, cgltf_component_type_r_32f>(
				mesh.StreamOfVertices.Attributes[EVertexAttributes::Normals],
				mesh.StreamOfVertices.AttributeStrides[EVertexAttributes::Normals], accessor); break;

			case cgltf_attribute_type_texcoord: FillBytesFromAccessor<Math::Vector2, cgltf_component_type_r_32f>(
				mesh.StreamOfVertices.Attributes[EVertexAttributes::TextureCoords],
				mesh.StreamOfVertices.AttributeStrides[EVertexAttributes::TextureCoords], accessor); break;

			case cgltf_attribute_type_tangent: FillFromAccessor<Math::Vector4, cgltf_component_type_r_32f>(gltfTangents, accessor); break;
			default: break; // just skip
			}

			uint32_t numVertices = mesh.StreamOfVertices.NumVertices;
			uint32_t numIndices = static_cast<uint32_t>(mesh.Indices.size());

			// Sanity checks
			WARP_ASSERT(numVertices > 0);
			// TODO: Currently only with indices. If changing this, also check for creation of mesh.IndexBuffer (we assert there that indices exist)
			WARP_ASSERT(!mesh.Indices.empty());
			WARP_ASSERT(!mesh.StreamOfVertices.Attributes[EVertexAttributes::Positions].empty());
			for (size_t i = 0; i < EVertexAttributes::NumAttributes; ++i)
			{
				// If empty - no attribute
				if (mesh.StreamOfVertices.Attributes[i].empty())
				{
					continue;
				}

				WARP_MAYBE_UNUSED size_t numAttributes = mesh.StreamOfVertices.Attributes[i].size() / mesh.StreamOfVertices.AttributeStrides[i];
				WARP_ASSERT(numVertices == numAttributes);
			}
			WARP_ASSERT(numIndices % 3 == 0);

			// Process tangents/bitangents
			if (!gltfTangents.empty())
			{
				// TODO: Remove redundant copying of tangents, we can write immediately into the mesh.StreamOfVertices
				std::vector<Math::Vector3> tangents(numVertices);
				std::vector<Math::Vector3> bitangents(numVertices);
				static constexpr size_t TangentStride = sizeof(Math::Vector3);

				size_t normalStride = mesh.StreamOfVertices.AttributeStrides[EVertexAttributes::Normals];
				WARP_ASSERT(normalStride == sizeof(Math::Vector3)); // TODO: One more sanity check, just in case. Remove it 

				// If tangents non-empty, assume there are numVertices of them
				for (size_t i = 0; i < numVertices; ++i)
				{
					// Get normal from byte array
					Math::Vector3& normal = reinterpret_cast<Math::Vector3&>(mesh.StreamOfVertices.Attributes[EVertexAttributes::Normals][i * normalStride]);
					tangents[i] = Math::Vector3(gltfTangents[i]);
					bitangents[i] = normal.Cross(tangents[i]) * gltfTangents[i].w;
				}

				size_t bytesToCopy = TangentStride * numVertices;

				auto& streamOfTangents = mesh.StreamOfVertices.Attributes[EVertexAttributes::Tangents];
				streamOfTangents.clear();
				streamOfTangents.resize(bytesToCopy);
				std::memcpy(streamOfTangents.data(), tangents.data(), bytesToCopy);

				auto& streamOfBitangents = mesh.StreamOfVertices.Attributes[EVertexAttributes::Bitangents];
				streamOfBitangents.clear();
				streamOfBitangents.resize(bytesToCopy);
				std::memcpy(streamOfBitangents.data(), bitangents.data(), bytesToCopy);

				mesh.StreamOfVertices.AttributeStrides[EVertexAttributes::Tangents] = TangentStride;
				mesh.StreamOfVertices.AttributeStrides[EVertexAttributes::Bitangents] = TangentStride;
			}
		}
	}

	void ModelLoader::ComputeMeshletsAndOptimize(StaticMesh& mesh)
	{
		// TODO: https://github.com/microsoft/DirectXMesh/wiki/DirectXMesh
		// Add mesh optimization

		Math::Vector3* meshPositions = reinterpret_cast<Math::Vector3*>(mesh.StreamOfVertices.Attributes[EVertexAttributes::Positions].data());
		WARP_MAYBE_UNUSED HRESULT hr = DirectX::ComputeMeshlets(
			mesh.Indices.data(), mesh.NumIndices / 3,
			meshPositions, mesh.StreamOfVertices.NumVertices,
			nullptr,
			mesh.Meshlets, mesh.UniqueVertexIndices, mesh.PrimitiveIndices);

		WARP_ASSERT(SUCCEEDED(hr), "Failed to compute meshlets for mesh");
	}

	void ModelLoader::UploadMeshResources(StaticMesh& mesh)
	{
		// TODO: REWRITE RHI RESOURCE HANDLING;
		Renderer* renderer = Application::Get().GetRenderer();
		RHIDevice* Device = renderer->GetDevice();

		// Fill upload resources with data
		static constexpr uint32_t IndexStride = sizeof(uint32_t);
		const size_t indicesInBytes = mesh.NumIndices * IndexStride;
		RHIBuffer uploadIndexBuffer(Device, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE, IndexStride, indicesInBytes);

		std::memcpy(uploadIndexBuffer.GetCpuVirtualAddress<std::byte>(), mesh.Indices.data(), indicesInBytes);

		// Asset that we have indices
		mesh.IndexBuffer = RHIBuffer(Device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE, IndexStride, indicesInBytes);

		StaticMesh::VertexStream::AttributeArray<RHIBuffer> uploadResources;
		for (size_t i = 0; i < EVertexAttributes::NumAttributes; ++i)
		{
			// Skip if no attribute
			if (mesh.StreamOfVertices.Attributes[i].empty())
			{
				continue;
			}

			uint32_t stride = mesh.StreamOfVertices.AttributeStrides[i];
			size_t sizeInBytes = mesh.StreamOfVertices.NumVertices * stride;
			uploadResources[i] = RHIBuffer(Device, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE, stride, sizeInBytes);

			std::memcpy(uploadResources[i].GetCpuVirtualAddress<std::byte>(), mesh.StreamOfVertices.Attributes[i].data(), sizeInBytes);

			mesh.StreamOfVertices.Resources[i] = RHIBuffer(Device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE, stride, sizeInBytes);
		}

		static constexpr uint32_t MeshletStride = sizeof(Meshlet);
		static constexpr uint32_t UVIndexStride = sizeof(uint8_t);
		static constexpr uint32_t PrimitiveIndicesStride = sizeof(MeshletTriangle);

		size_t meshletsInBytes = mesh.Meshlets.size() * sizeof(Meshlet);
		size_t uniqueVertexIndicesInBytes = mesh.UniqueVertexIndices.size() * UVIndexStride;
		size_t primitiveIndicesInBytes = mesh.PrimitiveIndices.size() * PrimitiveIndicesStride;

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

		std::memcpy(uploadMeshletBuffer.GetCpuVirtualAddress<std::byte>(), mesh.Meshlets.data(), meshletsInBytes);
		std::memcpy(uploadUniqueVertexIndicesBuffer.GetCpuVirtualAddress<std::byte>(), mesh.UniqueVertexIndices.data(), uniqueVertexIndicesInBytes);
		std::memcpy(uploadPrimitiveIndicesBuffer.GetCpuVirtualAddress<std::byte>(), mesh.PrimitiveIndices.data(), primitiveIndicesInBytes);

		// Note of the FIX with "Wrong resource state"
		// The COPY flags (COPY_DEST and COPY_SOURCE) used as initial states represent states in the 3D/Compute type class. 
		// To use a resource initially on a Copy queue it should start in the COMMON state. 
		// The COMMON state can be used for all usages on a Copy queue using the implicit state transitions. 
		mesh.MeshletBuffer = RHIBuffer(Device,
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_FLAG_NONE, MeshletStride, meshletsInBytes);
		mesh.UniqueVertexIndicesBuffer = RHIBuffer(Device,
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_FLAG_NONE, UVIndexStride, uniqueVertexIndicesInBytes);
		mesh.PrimitiveIndicesBuffer = RHIBuffer(Device,
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
			copyContext.CopyResource(&mesh.IndexBuffer, &uploadIndexBuffer);
			for (size_t i = 0; i < EVertexAttributes::NumAttributes; ++i)
			{
				// Skip if no attribute
				if (mesh.StreamOfVertices.Attributes[i].empty())
				{
					continue;
				}

				copyContext.CopyResource(&mesh.StreamOfVertices.Resources[i], &uploadResources[i]);
			}

			copyContext.CopyResource(&mesh.MeshletBuffer, &uploadMeshletBuffer);
			copyContext.CopyResource(&mesh.UniqueVertexIndicesBuffer, &uploadUniqueVertexIndicesBuffer);
			copyContext.CopyResource(&mesh.PrimitiveIndicesBuffer, &uploadPrimitiveIndicesBuffer);
		}
		copyContext.Close();
		copyContext.Execute(true); // TODO: We wait for completion, though shouldnt
	}

	Math::Matrix ModelLoader::GetLocalToModel(cgltf_node* node)
	{
		// Get transform of a node
		// We use right-handed coordinate system, as do glTF
		Math::Matrix LocalToModel;
		if (node->has_matrix)
		{
			LocalToModel = Math::Matrix(node->matrix);
		}
		else
		{
			Math::Matrix T = Math::Matrix::CreateTranslation(node->has_translation ? Math::Vector3(node->translation) : Math::Vector3());
			Math::Matrix R = Math::Matrix::CreateFromQuaternion(node->has_rotation ? Math::Quaternion(node->rotation) : Math::Quaternion());
			Math::Matrix S = Math::Matrix::CreateScale(node->has_scale ? Math::Vector3(node->scale) : Math::Vector3(1.0f));
			LocalToModel = S * R * T;
		}
		return LocalToModel;
	}

}