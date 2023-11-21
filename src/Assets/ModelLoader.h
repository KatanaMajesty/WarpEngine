#pragma once

#include <string_view>

#include "Asset.h"
#include "AssetManager.h"
#include "../Math/Math.h"
#include "../Core/Defines.h"

struct cgltf_node;

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

		AssetManager* m_assetManager;
	};

}