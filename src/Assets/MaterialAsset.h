#pragma once

#pragma once

#include "Asset.h"
#include "../Math/Math.h"

namespace Warp
{

	struct MaterialAsset : Asset
	{
		static constexpr EAssetType StaticType = EAssetType::Material;

		MaterialAsset(uint32_t ID) : Asset(ID, EAssetType::Material) {}

		bool HasAlbedoMap() const { return AlbedoMap.IsValid(); }
		bool HasNormalMap() const { return NormalMap.IsValid(); }
		bool HasRoughnessMetalnessMap() const { return RoughnessMetalnessMap.IsValid(); }

		AssetProxy AlbedoMap;
		AssetProxy NormalMap;
		AssetProxy RoughnessMetalnessMap;

		Math::Vector4 Albedo;
		Math::Vector2 RoughnessMetalness;
	};

}