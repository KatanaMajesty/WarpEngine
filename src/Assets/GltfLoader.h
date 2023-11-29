#pragma once

#include <string>
#include <vector>

#include "Asset.h"
#include "AssetManager.h"
#include "MeshAsset.h"

struct cgltf_node;
struct cgltf_primitive;

namespace Warp
{

	class GltfLoader
	{
	public:
		std::vector<AssetProxy> LoadFromFile(std::string_view filepath, AssetManager* assetManager);

	private:
		void ProcessStaticMeshNode(std::vector<AssetProxy>& proxies, AssetManager* assetManager, cgltf_node* node);
		void ProcessStaticMeshAttributes(MeshAsset* mesh, const Math::Matrix& localToModel, cgltf_primitive* primitive);
	};

}