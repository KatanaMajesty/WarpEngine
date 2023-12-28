#include "MeshLoader.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <vector>
#include <cstring>
#include <filesystem>

#include "../../Math/Math.h"
#include "../../Core/Logger.h"
#include "../../Core/Assert.h"
#include "../MeshAsset.h"

// TODO: Warp Asset Loader does not support sparse GLTF accessors currently. Will be removed asap
#define WAL_GLTF_SPARSE_NOIMPL(accessor) { if (accessor->is_sparse) { WARP_YIELD_NOIMPL(); } }

namespace Warp::MeshLoader
{

	static Math::Matrix GetLocalToModel(cgltf_node* node)
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

	// Quickstart with glTF https://www.khronos.org/files/gltf20-reference-guide.pdf
	// Afterwards we chill here - https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html

	void ProcessStaticMeshNode(std::vector<Mesh>&, const std::filesystem::path& folder, const LoadDesc&, cgltf_node*);

	std::vector<Mesh> LoadFromGltfFile(std::string_view filepath, const LoadDesc& desc)
	{
		cgltf_data* data = nullptr;
		cgltf_options options = {};
		cgltf_result result = cgltf_parse_file(&options, filepath.data(), &data);
		if (result != cgltf_result_success)
		{
			WARP_LOG_ERROR("Failed to parse GLTF model at {}", filepath);
			return {};
		}

		// Load bin buffers as well
		result = cgltf_load_buffers(&options, data, filepath.data());
		if (result != cgltf_result_success)
		{
			WARP_LOG_ERROR("Failed to parse buffers for GLTF model at {}", filepath);
			return {};
		}

		std::vector<Mesh> meshes;
		std::filesystem::path folder = std::filesystem::path(filepath).parent_path();
		for (size_t i = 0; i < data->scenes_count; ++i)
		{
			cgltf_scene& scene = data->scenes[i];

			// We just process each node as if it is a static mesh node
			for (size_t nodeIndex = 0; nodeIndex < scene.nodes_count; ++nodeIndex)
			{
				cgltf_node* node = scene.nodes[nodeIndex];

				// Process mesh
				ProcessStaticMeshNode(meshes, folder, desc, node);
			}
		}

		// TODO: Check if succeeded. If not - destroy asset proxy

		cgltf_free(data);
		return meshes;
	}

	void LoadImageFromView(ImageLoader::Image& outImage, const std::filesystem::path& folder, cgltf_image* img)
	{
		if (img->buffer_view)
		{
			// Load from memory
			WARP_YIELD_NOIMPL();
			const char* mimeType = img->mime_type;
		}
		else
		{
			WARP_ASSERT(img->uri);
			// TODO: Currently we do not support DDS extension, but we will definitely!
			std::string imagePath = (folder / img->uri).string();
			outImage = ImageLoader::LoadWICFromFile(imagePath, true);
			outImage.Name = img->name;
		}
	}

	void ProcessMaterial(Mesh& mesh, const std::filesystem::path& folder, cgltf_material* glTFMaterial)
	{
		WARP_ASSERT(glTFMaterial);
		if (glTFMaterial->has_pbr_metallic_roughness)
		{
			cgltf_pbr_metallic_roughness m = glTFMaterial->pbr_metallic_roughness;
			// If we hase BaseColorTexture - use it, otherwise use BaseColorFactor
			if (m.base_color_texture.texture != nullptr)
			{
				// Create BaseColor asset using texture loader
				cgltf_image* img = m.base_color_texture.texture->image;
				LoadImageFromView(mesh.Material.BaseColor, folder, img);
			}
			else
			{
				mesh.Material.BaseColorFactor = Math::Vector4(m.base_color_factor);
			}

			if (m.metallic_roughness_texture.texture != nullptr)
			{
				cgltf_image* img = m.metallic_roughness_texture.texture->image;
				LoadImageFromView(mesh.Material.MetalnessRoughnessMap, folder, img);
			}
			else
			{
				mesh.Material.MetalnessRoughnessFactor = Math::Vector2(m.metallic_factor, m.roughness_factor);
			}
		}

		if (glTFMaterial->normal_texture.texture != nullptr)
		{
			cgltf_image* img = glTFMaterial->normal_texture.texture->image;
			LoadImageFromView(mesh.Material.NormalMap, folder, img);
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

	void ProcessStaticMeshAttributes(Mesh& mesh, const Math::Matrix& localToModel, const LoadDesc& desc, cgltf_primitive* primitive)
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
				mesh.NumVertices = static_cast<uint32_t>(accessor->count);

				FillBytesFromAccessor<Math::Vector3, cgltf_component_type_r_32f>(
					mesh.Attributes[EVertexAttributes_Positions],
					mesh.AttributeStrides[EVertexAttributes_Positions], accessor);

				// According to spec: POSITION accessor MUST have its min and max properties defined.
				WARP_ASSERT(accessor->has_max && accessor->has_min);

				// TODO: Calculate center of the mesh, calculate the length of the mesh for AABB
			};  break;
			case cgltf_attribute_type_normal: FillBytesFromAccessor<Math::Vector3, cgltf_component_type_r_32f>(
				mesh.Attributes[EVertexAttributes_Normals],
				mesh.AttributeStrides[EVertexAttributes_Normals], accessor); break;

			case cgltf_attribute_type_texcoord: FillBytesFromAccessor<Math::Vector2, cgltf_component_type_r_32f>(
				mesh.Attributes[EVertexAttributes_TextureCoords],
				mesh.AttributeStrides[EVertexAttributes_TextureCoords], accessor); break;

			case cgltf_attribute_type_tangent: FillFromAccessor<Math::Vector4, cgltf_component_type_r_32f>(gltfTangents, accessor); break;
			default: break; // just skip
			}
		}

		uint32_t numVertices = mesh.NumVertices;
		uint32_t numIndices = static_cast<uint32_t>(mesh.Indices.size());

		// Sanity checks
		WARP_ASSERT(numVertices > 0 && numIndices > 0);
		// TODO: Currently only with indices. If changing this, also check for creation of mesh->IndexBuffer (we assert there that indices exist)
		for (size_t i = 0; i < EVertexAttributes_NumAttributes; ++i)
		{
			// check if mesh has attributes
			if (mesh.Attributes[i].empty())
			{
				continue;
			}

			WARP_MAYBE_UNUSED size_t numAttributes = mesh.Attributes[i].size() / mesh.AttributeStrides[i];
			WARP_ASSERT(numVertices == numAttributes);
		}
		WARP_ASSERT(numIndices % 3 == 0); // triangles only!
		WARP_ASSERT(mesh.AttributeStrides[EVertexAttributes_Normals] == sizeof(Math::Vector3)); // TODO: One more sanity check, just in case. Remove it 

		Math::Vector3* meshPositions = reinterpret_cast<Math::Vector3*>(mesh.Attributes[EVertexAttributes_Positions].data());
		Math::Vector3* meshNormals = reinterpret_cast<Math::Vector3*>(mesh.Attributes[EVertexAttributes_Normals].data());

		// Process tangents/bitangents
		bool ableToGenerateTangents = (desc.GenerateTangents && !mesh.Attributes[EVertexAttributes_TextureCoords].empty());
		if (!gltfTangents.empty() || ableToGenerateTangents)
		{
			static constexpr size_t TangentStride = sizeof(Math::Vector3);
			size_t bytesToCopy = TangentStride * numVertices;

			std::vector<std::byte>& streamOfTangents = mesh.Attributes[EVertexAttributes_Tangents];
			streamOfTangents.clear();
			streamOfTangents.resize(bytesToCopy);

			std::vector<std::byte>& streamOfBitangents = mesh.Attributes[EVertexAttributes_Bitangents];
			streamOfBitangents.clear();
			streamOfBitangents.resize(bytesToCopy);

			mesh.AttributeStrides[EVertexAttributes_Tangents] = TangentStride;
			mesh.AttributeStrides[EVertexAttributes_Bitangents] = TangentStride;

			Math::Vector3* meshTangents = reinterpret_cast<Math::Vector3*>(streamOfTangents.data());
			Math::Vector3* meshBitangents = reinterpret_cast<Math::Vector3*>(streamOfBitangents.data());

			if (!gltfTangents.empty())
			{
				// If tangents non-empty, assume there are numVertices of them and there are tex uvs ofcourse
				WARP_ASSERT(!mesh.Attributes[EVertexAttributes_TextureCoords].empty());
				WARP_ASSERT(static_cast<uint32_t>(gltfTangents.size()) == numVertices)
				for (size_t i = 0; i < numVertices; ++i)
				{
					// Get normal from byte array
					Math::Vector3& normal = meshNormals[i];
					meshTangents[i] = Math::Vector3(gltfTangents[i]);
					meshBitangents[i] = normal.Cross(meshTangents[i]) * gltfTangents[i].w;
				}
			}
			else // If we requested tangents to be generated - generate them here
			{
				Math::Vector2* meshUvs = reinterpret_cast<Math::Vector2*>(mesh.Attributes[EVertexAttributes_TextureCoords].data());

				uint32_t numPrimitives = numIndices / 3;
				for (uint32_t i = 0; i < numPrimitives; ++i)
				{
					uint32_t i0 = mesh.Indices[i * 3 + 0];
					uint32_t i1 = mesh.Indices[i * 3 + 1];
					uint32_t i2 = mesh.Indices[i * 3 + 2];

					const Math::Vector3& pos0 = meshPositions[i0];
					const Math::Vector3& pos1 = meshPositions[i1];
					const Math::Vector3& pos2 = meshPositions[i2];
					Math::Vector3 deltaPos1 = pos1 - pos0;
					Math::Vector3 deltaPos2 = pos2 - pos0;

					const Math::Vector2& uv0 = meshUvs[i0];
					const Math::Vector2& uv1 = meshUvs[i1];
					const Math::Vector2& uv2 = meshUvs[i2];
					Math::Vector2 deltaUv1 = uv1 - uv0;
					Math::Vector2 deltaUv2 = uv2 - uv0;

					float r = 1.0f / (deltaUv1.x * deltaUv2.y - deltaUv1.y * deltaUv2.x);
					Math::Vector3 tangent = (deltaPos1 * deltaUv2.y - deltaPos2 * deltaUv1.y) * r;
					Math::Vector3 bitangent = (deltaPos2 * deltaUv1.x - deltaPos1 * deltaUv2.x) * r;

					meshTangents[i0] += tangent;
					meshTangents[i1] += tangent;
					meshTangents[i2] += tangent;
					meshBitangents[i0] += bitangent;
					meshBitangents[i1] += bitangent;
					meshBitangents[i2] += bitangent;
				}

				for (uint32_t i = 0; i < numVertices; ++i)
				{
					meshTangents[i].Normalize();
					meshBitangents[i].Normalize();
				}
			}
		}

		// Transform vertices from local to model space
		XMVector3TransformCoordStream(
			meshPositions, sizeof(Math::Vector3),
			meshPositions, sizeof(Math::Vector3),
			numVertices, localToModel);

		// Check if mesh has normals
		if (!mesh.Attributes[EVertexAttributes_Normals].empty())
		{
			XMVector3TransformNormalStream(
				meshNormals, sizeof(Math::Vector3),
				meshNormals, sizeof(Math::Vector3),
				numVertices, localToModel);
		}
	}

	void ProcessStaticMeshNode(std::vector<Mesh>& meshes, const std::filesystem::path& folder, const LoadDesc& desc, cgltf_node* node)
	{
		Math::Matrix LocalToModel = GetLocalToModel(node);

		cgltf_mesh* glTFMsh = node->mesh;
		WARP_ASSERT(glTFMsh, "Damaged glTF mesh?");

		for (size_t primitiveIndex = 0; primitiveIndex < glTFMsh->primitives_count; ++primitiveIndex)
		{
			Mesh& mesh = meshes.emplace_back();

			mesh.Name = fmt::format("{}_{}", glTFMsh->name, primitiveIndex);
			cgltf_primitive& primitive = glTFMsh->primitives[primitiveIndex];
			cgltf_primitive_type type = primitive.type;

			cgltf_material* glTFMaterial = primitive.material;
			ProcessMaterial(mesh, folder, glTFMaterial);

			// We only use triangles for now. Will be removed in future. This is just in case
			WARP_ASSERT(type == cgltf_primitive_type_triangles);
			ProcessStaticMeshAttributes(mesh, LocalToModel, desc, &primitive);
		}

		// Process all its children
		for (size_t i = 0; i < node->children_count; ++i)
		{
			ProcessStaticMeshNode(meshes, folder, desc, node->children[i]);
		}
	}

	

}
