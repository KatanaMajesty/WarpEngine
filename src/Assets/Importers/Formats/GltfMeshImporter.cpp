#include "../MeshImporter.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

// TODO: Warp Asset Loader does not support sparse GLTF accessors currently. Will be removed asap
#define WAL_GLTF_SPARSE_NOIMPL(accessor) { if (accessor->is_sparse) { WARP_YIELD_NOIMPL(); } }

#include <cstring>
#include <filesystem>
#include <array>
#include <string>
#include <vector>

#include "../../../Util/String.h"

#include "../../AssetManager.h"
#include "../../MaterialAsset.h"
#include "../../MeshAsset.h"

#include "../../../Math/Math.h"
#include "../../../Util/Logger.h"
#include "../../../Core/Assert.h"
#include "../../../Renderer/Vertex.h"

namespace Warp
{

    namespace GltfImporter
    {
        // Quickstart with glTF https://www.khronos.org/files/gltf20-reference-guide.pdf
        // Afterwards we chill here - https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
        // 
        // TODO: 29/02/24 -> Left of here. What I wanted to implement is texture caching as we run out of ram instantly. MeshLoader currently is not 
        // written in a way to support texture caching, as this was intended for MeshImporter, but I was wrong by doing so... Write caching please

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

            bool IsValid() const { return !Submeshes.empty() && Submeshes.size() == SubmeshMaterials.size(); }

            std::string Name;
            std::vector<Submesh> Submeshes;
            std::vector<AssetProxy> SubmeshMaterials;
        };

        struct StaticMeshImportDesc
        {
            // Determines whether or not to generate tangents and bitangents (binormals)
            bool GenerateTangents = false;
        };

        static Math::Matrix GetLocalToModel(cgltf_node* node);

        static AssetProxy StaticMesh_ImportTextureFromView(const std::filesystem::path& folder, cgltf_image* img, TextureImporter* importer);
        static AssetProxy StaticMesh_ImportSubmeshMaterial(const std::filesystem::path& folder, cgltf_material* glTFMaterial, TextureImporter* importer);

        template<typename AttributeType, cgltf_component_type ReqType>
        static void StaticMesh_FillAttributesFromAccessor(std::vector<AttributeType>& dest, cgltf_accessor* accessor);

        template<typename AttributeType, cgltf_component_type ReqType>
        static void StaticMesh_FillAttributeBytesFromAccessor(std::vector<std::byte>& dest, uint32_t& stride, cgltf_accessor* accessor);

        static void StaticMesh_ProcessAttributes(StaticMesh::Submesh& submesh, const Math::Matrix& localToModel, const StaticMeshImportDesc& desc, cgltf_primitive* primitive);
        static void StaticMesh_ProcessNode(StaticMesh& mesh, const std::filesystem::path& folder, const StaticMeshImportDesc& desc, cgltf_node* node, TextureImporter* importer);

        Math::Matrix GetLocalToModel(cgltf_node* node)
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

        AssetProxy StaticMesh_ImportTextureFromView(const std::filesystem::path& folder, cgltf_image* img, TextureImporter* importer)
        {
            if (!importer || !img)
            {
                return AssetProxy();
            }

            if (img->buffer_view)
            {
                // Load from memory (glb meshes)
                WARP_YIELD_NOIMPL();
                const char* mimeType = img->mime_type;
                return AssetProxy();
            }
            else
            {
                WARP_ASSERT(img->uri);
                std::string imagePath = (folder / img->uri).string();

                // TODO: GenerateMips is always true? How to get around this one?
                AssetProxy proxy = importer->ImportFromFile(imagePath, TextureImportDesc{ .GenerateMips = true });
                if (!proxy.IsValid())
                {
                    WARP_LOG_ERROR("GltfImporter::StaticMesh_ImportTextureFromView -> Failed to import a texture \'{}\' from view", imagePath);
                }

                return proxy;
            }
        }

        // TODO: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html - reevaluate metallic-roughness
        // TODO (01.04.2024): Would be nice to revisit this one. What if we have more scene formats supported (fbx, obj, etc.)? Would it be appropriate
        // To write a separate material importer and pass it here rather than passing raw AssetManager?
        AssetProxy StaticMesh_ImportSubmeshMaterial(const std::filesystem::path& folder, cgltf_material* glTFMaterial, TextureImporter* importer)
        {
            if (!importer || !importer->GetAssetManager() || !glTFMaterial)
            {
                return AssetProxy();
            }

            if (!glTFMaterial->has_pbr_metallic_roughness)
            {
                WARP_LOG_ERROR("GltfImporter::StaticMesh_ImportSubmeshMaterial -> Unsupported glTF material model");
                return AssetProxy();
            }

            AssetManager* manager = importer->GetAssetManager();
            AssetProxy proxy = manager->CreateAsset<MaterialAsset>();
            if (!proxy.IsValid())
            {
                WARP_LOG_ERROR("GltfImporter::StaticMesh_ImportSubmeshMaterial -> Failed to create a material asset");
                return proxy;
            }

            MaterialAsset* material = manager->GetAs<MaterialAsset>(proxy);

            cgltf_pbr_metallic_roughness m = glTFMaterial->pbr_metallic_roughness;
            // If we have albedo texture - use it, otherwise use albedo factor
            if (m.base_color_texture.texture != nullptr)
            {
                // Create BaseColor asset using texture loader
                cgltf_image* img = m.base_color_texture.texture->image;
                material->AlbedoMap = StaticMesh_ImportTextureFromView(folder, img, importer);
            }
            else
            {
                material->Albedo = Math::Vector4(m.base_color_factor);
            }

            if (m.metallic_roughness_texture.texture != nullptr)
            {
                // As of glTF 2.0 metallic-roughness texture is RGBA texture with green channel for roughness and blue for metalness
                // thus it is g/b -> roughness/metalness
                cgltf_image* img = m.metallic_roughness_texture.texture->image;
                material->RoughnessMetalnessMap = StaticMesh_ImportTextureFromView(folder, img, importer);
            }
            else
            {
                material->RoughnessMetalness = Math::Vector2(m.roughness_factor, m.metallic_factor);
            }

            if (glTFMaterial->normal_texture.texture != nullptr)
            {
                cgltf_image* img = glTFMaterial->normal_texture.texture->image;
                material->NormalMap = StaticMesh_ImportTextureFromView(folder, img, importer);
            }

            return proxy;
        }

        template<typename AttributeType, cgltf_component_type ReqType>
        void StaticMesh_FillAttributesFromAccessor(std::vector<AttributeType>& dest, cgltf_accessor* accessor)
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
        void StaticMesh_FillAttributeBytesFromAccessor(std::vector<std::byte>& dest, uint32_t& stride, cgltf_accessor* accessor)
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

        void StaticMesh_ProcessAttributes(StaticMesh::Submesh& submesh, const Math::Matrix& localToModel, const StaticMeshImportDesc& desc, cgltf_primitive* primitive)
        {
            cgltf_accessor* indices = primitive->indices;
            if (!indices)
            {
                // Indices property is not defined
                // Number of vertex indices to render is defined by the count property of attribute accessors (with the implied values from range [0..count))
                WARP_LOG_FATAL("GltfMeshLoader -> no indices property!");
                WARP_YIELD_NOIMPL(); // We do not support non-indexed geometry currently
            }
            else
            {
                WAL_GLTF_SPARSE_NOIMPL(indices);
                if (indices->component_type == cgltf_component_type_r_16u)
                {
                    // if u16, manually convert u16 to u32
                    std::vector<uint16_t> indices16;
                    StaticMesh_FillAttributesFromAccessor<uint16_t, cgltf_component_type_r_16u>(indices16, indices);
                    submesh.Indices.reserve(indices16.size());
                    for (uint16_t i : indices16)
                    {
                        submesh.Indices.push_back(static_cast<uint32_t>(i));
                    }
                }
                else StaticMesh_FillAttributesFromAccessor<uint32_t, cgltf_component_type_r_32u>(submesh.Indices, indices);
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
                    submesh.NumVertices = static_cast<uint32_t>(accessor->count);

                    StaticMesh_FillAttributeBytesFromAccessor<Math::Vector3, cgltf_component_type_r_32f>(
                        submesh.Attributes[eVertexAttribute_Positions],
                        submesh.AttributeStrides[eVertexAttribute_Positions], accessor);

                    // According to spec: POSITION accessor MUST have its min and max properties defined.
                    WARP_ASSERT(accessor->has_max && accessor->has_min);

                    // TODO: Calculate center of the mesh, calculate the length of the mesh for AABB
                };  break;
                case cgltf_attribute_type_normal: StaticMesh_FillAttributeBytesFromAccessor<Math::Vector3, cgltf_component_type_r_32f>(
                    submesh.Attributes[eVertexAttribute_Normals],
                    submesh.AttributeStrides[eVertexAttribute_Normals], accessor); break;

                case cgltf_attribute_type_texcoord: StaticMesh_FillAttributeBytesFromAccessor<Math::Vector2, cgltf_component_type_r_32f>(
                    submesh.Attributes[eVertexAttribute_TextureCoords],
                    submesh.AttributeStrides[eVertexAttribute_TextureCoords], accessor); break;

                case cgltf_attribute_type_tangent: StaticMesh_FillAttributesFromAccessor<Math::Vector4, cgltf_component_type_r_32f>(gltfTangents, accessor); break;
                default: break; // just skip
                }
            }

            uint32_t numVertices = submesh.NumVertices;
            uint32_t numIndices = static_cast<uint32_t>(submesh.Indices.size());

            // Sanity checks
            WARP_ASSERT(numVertices > 0 && numIndices > 0);
            // TODO: Currently only with indices. If changing this, also check for creation of submesh->IndexBuffer (we assert there that indices exist)
            for (size_t i = 0; i < eVertexAttribute_NumAttributes; ++i)
            {
                // check if submesh has attributes
                if (submesh.Attributes[i].empty())
                {
                    continue;
                }

                WARP_MAYBE_UNUSED size_t numAttributes = submesh.Attributes[i].size() / submesh.AttributeStrides[i];
                WARP_ASSERT(numVertices == numAttributes);
            }
            WARP_ASSERT(numIndices % 3 == 0); // triangles only!
            WARP_ASSERT(submesh.AttributeStrides[eVertexAttribute_Normals] == sizeof(Math::Vector3)); // TODO: One more sanity check, just in case. Remove it 

            Math::Vector3* meshPositions = reinterpret_cast<Math::Vector3*>(submesh.Attributes[eVertexAttribute_Positions].data());
            Math::Vector3* meshNormals = reinterpret_cast<Math::Vector3*>(submesh.Attributes[eVertexAttribute_Normals].data());

            // Process tangents/bitangents
            bool ableToGenerateTangents = (desc.GenerateTangents && !submesh.Attributes[eVertexAttribute_TextureCoords].empty());
            if (!gltfTangents.empty() || ableToGenerateTangents)
            {
                static constexpr size_t TangentStride = sizeof(Math::Vector3);
                size_t bytesToCopy = TangentStride * numVertices;

                std::vector<std::byte>& streamOfTangents = submesh.Attributes[eVertexAttribute_Tangents];
                streamOfTangents.clear();
                streamOfTangents.resize(bytesToCopy);

                std::vector<std::byte>& streamOfBitangents = submesh.Attributes[eVertexAttribute_Bitangents];
                streamOfBitangents.clear();
                streamOfBitangents.resize(bytesToCopy);

                submesh.AttributeStrides[eVertexAttribute_Tangents] = TangentStride;
                submesh.AttributeStrides[eVertexAttribute_Bitangents] = TangentStride;

                Math::Vector3* meshTangents = reinterpret_cast<Math::Vector3*>(streamOfTangents.data());
                Math::Vector3* meshBitangents = reinterpret_cast<Math::Vector3*>(streamOfBitangents.data());

                if (!gltfTangents.empty())
                {
                    // If tangents non-empty, assume there are numVertices of them and there are tex uvs ofcourse
                    WARP_ASSERT(!submesh.Attributes[eVertexAttribute_TextureCoords].empty());
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
                    Math::Vector2* meshUvs = reinterpret_cast<Math::Vector2*>(submesh.Attributes[eVertexAttribute_TextureCoords].data());

                    uint32_t numPrimitives = numIndices / 3;
                    for (uint32_t i = 0; i < numPrimitives; ++i)
                    {
                        uint32_t i0 = submesh.Indices[i * 3 + 0];
                        uint32_t i1 = submesh.Indices[i * 3 + 1];
                        uint32_t i2 = submesh.Indices[i * 3 + 2];

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
            if (!submesh.Attributes[eVertexAttribute_Normals].empty())
            {
                XMVector3TransformNormalStream(
                    meshNormals, sizeof(Math::Vector3),
                    meshNormals, sizeof(Math::Vector3),
                    numVertices, localToModel);
            }
        }

        void StaticMesh_ProcessNode(StaticMesh& mesh, const std::filesystem::path& folder, const StaticMeshImportDesc& desc, cgltf_node* node, TextureImporter* importer)
        {
            Math::Matrix LocalToModel = GetLocalToModel(node);

            cgltf_mesh* glTFMsh = node->mesh;
            if (glTFMsh)
            {
                if (node->name)
                {
                    WARP_LOG_INFO("GltfMeshLoader -> Processing glTF mesh node \'{}\'", node->name);
                }

                // TODO: What if two different nodes? Will take the name of the latest one?
                // TODO: 05.03.24 -> I used "Unknown" as a placeholder if we dont have a name. Handle this in a better way please
                mesh.Name = glTFMsh->name ? glTFMsh->name : "Unknown";
                for (size_t primitiveIndex = 0; primitiveIndex < glTFMsh->primitives_count; ++primitiveIndex)
                {
                    StaticMesh::Submesh& submesh = mesh.Submeshes.emplace_back();
                    AssetProxy& submeshMaterial = mesh.SubmeshMaterials.emplace_back(); // We always emplace back a material, even if none
                    submesh.Properties = StaticMesh::eSubmeshProperty_None;

                    cgltf_primitive& primitive = glTFMsh->primitives[primitiveIndex];
                    cgltf_primitive_type type = primitive.type;
                    cgltf_material* glTFMaterial = primitive.material;
                    if (glTFMaterial)
                    {
                        submesh.Properties |= StaticMesh::eSubmeshProperty_HasMaterial;
                        submeshMaterial = StaticMesh_ImportSubmeshMaterial(folder, glTFMaterial, importer);
                    }

                    // We only use triangles for now. Will be removed in FAR future. This is just in case
                    WARP_ASSERT(type == cgltf_primitive_type_triangles);
                    StaticMesh_ProcessAttributes(submesh, LocalToModel, desc, &primitive);
                }
            }
            else
            {
                if (node->name)
                {
                    WARP_LOG_WARN("GltfMeshLoader -> Node \'{}\' is not a glTF mesh -> Skipping", node->name);
                }
            }

            // Process all its children
            for (size_t i = 0; i < node->children_count; ++i)
            {
                StaticMesh_ProcessNode(mesh, folder, desc, node->children[i], importer);
            }
        }

        void StaticMesh_ImportFromFile(const std::string& filepath, StaticMesh& mesh, const StaticMeshImportDesc& importDesc, TextureImporter* importer)
        {
            cgltf_data* data = nullptr;
            cgltf_options options = {};
            cgltf_result result = cgltf_parse_file(&options, filepath.data(), &data);
            if (result != cgltf_result_success)
            {
                WARP_LOG_ERROR("Failed to parse GLTF model at {}", filepath);
                return;
            }

            // Load bin buffers as well
            result = cgltf_load_buffers(&options, data, filepath.data());
            if (result != cgltf_result_success)
            {
                WARP_LOG_ERROR("Failed to parse buffers for GLTF model at {}", filepath);
                return;
            }

            std::filesystem::path folder = std::filesystem::path(filepath).parent_path();
            for (size_t i = 0; i < data->scenes_count; ++i)
            {
                cgltf_scene& scene = data->scenes[i];

                // TODO: Temporary we only support 1 mesh per scene. (And we actually also support 1 scene as well per gltf) 
                // I was to lazy to figure out the right implementation so bear with it when its back
                WARP_ASSERT(data->scenes_count == 1/* && scene.nodes_count == 1*/);

                // We just process each node as if it is a static mesh node
                for (size_t nodeIndex = 0; nodeIndex < scene.nodes_count; ++nodeIndex)
                {
                    cgltf_node* node = scene.nodes[nodeIndex];

                    // Process mesh
                    GltfImporter::StaticMesh_ProcessNode(mesh, folder, importDesc, node, importer);
                }
            }

            cgltf_free(data);
        }
    }

    AssetProxy MeshImporter::ImportStaticMeshFromGltfFile(const std::string& filepath)
    {
        AssetManager* manager = GetAssetManager();
        AssetProxy proxy = manager->GetAssetProxy(filepath);
        if (proxy.IsValid())
        {
            WARP_ASSERT(proxy.Type == EAssetType::Mesh);
            WARP_LOG_INFO("MeshImporter::ImportStaticMeshFromGltfFile -> Returning cached asset at \'{}\'", filepath);
            return proxy;
        }

        proxy = manager->CreateAsset<MeshAsset>(filepath);
        if (!proxy.IsValid())
        {
            WARP_LOG_ERROR("MeshImporter::ImportStaticMeshFromGltfFile -> Failed to create mesh asset for static glTF mesh at \'{}\'", filepath);
            return proxy;
        }

        MeshAsset* mesh = manager->GetAs<MeshAsset>(proxy);

        // TODO: StaticMeshImportDesc is hardcoded and predefined now -> maybe change it? 
        GltfImporter::StaticMesh importedMesh;
        GltfImporter::StaticMeshImportDesc desc = GltfImporter::StaticMeshImportDesc{ .GenerateTangents = true };
        GltfImporter::StaticMesh_ImportFromFile(filepath, importedMesh, desc, &m_textureImporter);

        if (!importedMesh.IsValid())
        {
            WARP_LOG_ERROR("MeshImporter::ImportStaticMeshFromGltfFile -> Failed to import static mesh at \'{}\'", filepath);
            return proxy;
        }

        // Convertion GltfImporter::StaticMesh -> MeshAsset
        size_t numSubmeshes = importedMesh.Submeshes.size();
        mesh->Name = importedMesh.Name;
        mesh->Submeshes.reserve(numSubmeshes);
        mesh->SubmeshMaterials.reserve(numSubmeshes);

        // https://github.com/microsoft/DirectXMesh/wiki/DirectXMesh
        // TODO: Add mesh optimization

        // Process every submesh of imported mesh
        for (size_t submeshIndex = 0; submeshIndex < numSubmeshes; ++submeshIndex)
        {
            GltfImporter::StaticMesh::Submesh& srcSubmesh = importedMesh.Submeshes[submeshIndex];

            Submesh submesh;
            submesh.NumVertices = srcSubmesh.NumVertices;
            for (size_t attributeIndex = 0; attributeIndex < eVertexAttribute_NumAttributes; ++attributeIndex)
            {
                submesh.Attributes[attributeIndex] = std::move(srcSubmesh.Attributes[attributeIndex]);
                submesh.AttributeStrides[attributeIndex] = srcSubmesh.AttributeStrides[attributeIndex];
            }

            AssetProxy materialProxy = std::move(importedMesh.SubmeshMaterials[submeshIndex]);

            // Mesh optimization and meshlet generation
            uint32_t numIndices = static_cast<uint32_t>(srcSubmesh.Indices.size());
            uint32_t numVertices = submesh.GetNumVertices();
            uint32_t numFaces = numIndices / 3;
            uint32_t* indices = srcSubmesh.Indices.data();
            Math::Vector3* meshPositions = reinterpret_cast<Math::Vector3*>(submesh.Attributes[eVertexAttribute_Positions].data());

            bool isMeshValid = true;

            // Show warnings on mesh validation
#if 1
            std::wstring msgs;
            HRESULT hr = DirectX::Validate(indices, numFaces, numVertices, nullptr, DirectX::VALIDATE_DEFAULT, &msgs);
            if (FAILED(hr))
            {
                std::string errors = WStringToString(msgs);
                WARP_LOG_ERROR("Submesh meshlet validation error: {}", errors);

                if (hr == HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW))
                {
                    WARP_LOG_WARN("Arithmetic overflow here!");
                }

                isMeshValid = false;
            }
#else
            HRESULT hr = DirectX::Validate(indices, numFaces, numVertices, nullptr, DirectX::VALIDATE_DEFAULT, nullptr);
            if (FAILED(hr))
            {
                WARP_LOG_ERROR("Submesh meshlet validation error at submesh {}", submeshIndex);

                if (hr == HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW))
                {
                    WARP_LOG_WARN("Arithmetic overflow here!");
                }

                isMeshValid = false;
            }
#endif

            WARP_ASSERT(submesh.HasAttributes(eVertexAttribute_Positions));
            WARP_ASSERT(numIndices > 0);

            std::vector<uint32_t> adjacency(numIndices);

            if (isMeshValid)
            {
                hr = DirectX::GenerateAdjacencyAndPointReps(indices, numFaces, meshPositions, numVertices, 0.0f, nullptr, adjacency.data());
                if (FAILED(hr))
                {
                    isMeshValid = false;
                    WARP_LOG_ERROR("Failed to generate mesh adjacency");
                }
            }

            std::vector<uint32_t> duplicateVerts;
            hr = DirectX::Clean(indices, numFaces, numVertices, adjacency.data(), nullptr, duplicateVerts);
            if (FAILED(hr))
            {
                WARP_LOG_ERROR("Failed to clean a submesh");
            }

            std::vector<uint32_t> faceRemap(numFaces);
            hr = DirectX::OptimizeFaces(indices, numFaces, adjacency.data(), faceRemap.data());
            if (FAILED(hr))
            {
                WARP_LOG_ERROR("Failed to optimize faces");
            }

            hr = DirectX::ReorderIB(indices, numFaces, faceRemap.data());
            if (FAILED(hr))
            {
                WARP_LOG_ERROR("Failed to reorder indices");
            }

            std::vector<uint32_t> vertexRemap(numVertices);
            hr = DirectX::OptimizeVertices(indices, numFaces, numVertices, vertexRemap.data());
            if (FAILED(hr))
            {
                WARP_LOG_ERROR("Failed to optimize vertices");
            }

            hr = DirectX::FinalizeIB(indices, numFaces, vertexRemap.data(), numVertices);
            if (FAILED(hr))
            {
                WARP_LOG_ERROR("Failed to finalize indices");
            }

            for (size_t attributeIndex = 0; attributeIndex < eVertexAttribute_NumAttributes; ++attributeIndex)
            {

                std::byte* attributes = submesh.Attributes[attributeIndex].data();
                uint32_t attributeStride = submesh.AttributeStrides[attributeIndex];
                hr = DirectX::FinalizeVB(attributes, attributeStride, numVertices, vertexRemap.data());
                if (FAILED(hr))
                {
                    WARP_LOG_ERROR("Failed to finalize vertices at {}", attributeIndex);
                }
            }

            if (isMeshValid)
            {
                hr = DirectX::ComputeMeshlets(
                    indices, numIndices / 3,
                    meshPositions, submesh.GetNumVertices(),
                    adjacency.data(),
                    submesh.Meshlets, submesh.UniqueVertexIndices, submesh.PrimitiveIndices);

                if (FAILED(hr))
                {
                    isMeshValid = false;
                    WARP_LOG_ERROR("MeshImporter::ImportStaticMeshFromGltfFile -> Failed to compute meshlets for a static mesh \'{}\', submeshIndex {}", mesh->Name, submeshIndex);
                }
            }

            if (!isMeshValid)
            {
                // TODO: 05.03.24 -> We CANNOT delete asset proxies as they are SHARED! Meaning that if we delete 1 material proxy ALL others will become invalid!
                // This should not be the case and asset proxies should become ref-counted. Added to roadmap today
                // https://trello.com/c/wz9KmvlS/26-make-asset-proxies-ref-counted
                /*if (materialProxy.IsValid())
                {
                    WARP_LOG_INFO("Failed to validate a mesh -> Destroying material asset");
                    materialProxy = GetAssetManager()->DestroyAsset(materialProxy);
                }*/

                continue;
            }

            mesh->Submeshes.emplace_back(submesh);
            mesh->SubmeshMaterials.emplace_back(materialProxy);
        }

        // Shrink capacity to size, do not waste extra memory for no reason (This is static mesh)
        mesh->Submeshes.shrink_to_fit();
        mesh->SubmeshMaterials.shrink_to_fit();

        return proxy;
    }

}