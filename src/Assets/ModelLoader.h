#pragma once

#include <string_view>

#include "Asset.h"
#include "AssetManager.h"
#include "../Math/Math.h"
#include "../Core/Defines.h"

struct cgltf_node;
struct cgltf_primitive;

namespace Warp
{

	class ModelLoader
	{
	public:
		ModelLoader(AssetManager* assetManager)
			: m_assetManager(assetManager)
		{
		}

		WARP_ATTR_NODISCARD AssetProxy Load(std::string_view filepath);

	private:
		void ProcessStaticMeshNode(ModelAsset* model, cgltf_node* node);
		void ProcessStaticMeshAttributes(StaticMesh& mesh, const Math::Matrix& localToModel, cgltf_primitive* primitive);
		void ComputeMeshletsAndOptimize(StaticMesh& mesh);
		void UploadMeshResources(StaticMesh& mesh);

		Math::Matrix GetLocalToModel(cgltf_node* node);

		AssetManager* m_assetManager;
	};

}