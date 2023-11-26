#pragma once

#include "Asset.h"

#include "../Renderer/RHI/Resource.h"
#include "../Renderer/Mesh.h"

namespace Warp
{

	// TODO: Temp, will be removed
	

	struct ModelAsset final : public Asset
	{
	public:
		static constexpr EAssetType StaticType = EAssetType::Model;

		ModelAsset() : Asset(StaticType) {}

		StaticMesh& AddStaticMesh() { return Meshes.emplace_back(); }

		std::vector<StaticMesh> Meshes;
	};

}