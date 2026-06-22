#include "nsf_scene.h"

#include "common/assert.h"
#include "common/cross_log.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

namespace Warp
{

// NsfResult NsfAssetLibrary::LoadGltfSceneFromFile(NsfAsset* NsfAsset, const std::filesystem::path& filepath) noexcept
// {
//     std::string filepathStr = filepath.string();
//     cgltf_data* data = nullptr;
//     cgltf_options options{};
//     cgltf_result result = cgltf_parse_file(&options, filepathStr.c_str(), &data);
//     if (result != cgltf_result_success)
//     {
//         WARP_LOG_ERROR(LOGGER_DEFAULT, "Failed to parse glTF file at {}", filepathStr);
//         return NSF_RESULT_ERROR;
//     }
//     // TODO: Temporary we only support 1 mesh per scene. (And we actually also support 1 scene as well per gltf)
//     WARP_ASSERT(data->scenes_count == 1);
//     WARP_ASSERT(data->file_type == cgltf_file_type_glb, "Only GLB is currently supported");
//     // Load bin buffers as well
//     result = cgltf_load_buffers(&options, data, filepathStr.data());
//     if (result != cgltf_result_success)
//     {
//         WARP_LOG_ERROR(LOGGER_DEFAULT, "Failed to parse buffers for GLTF model at {}", filepathStr);
//         return NSF_RESULT_ERROR;
//     }
//     for (uint32_t sceneIndex = 0; sceneIndex < data->scenes_count; ++sceneIndex)
//     {
//         cgltf_scene& scene = data->scenes[sceneIndex];
//         for (size_t nodeIndex = 0; nodeIndex < scene.nodes_count; ++nodeIndex)
//         {
//             cgltf_node* node = scene.nodes[nodeIndex];
//         }
//     }
//     cgltf_free(data);
//     return NSF_RESULT_OK;
// }

} // Warp namespace