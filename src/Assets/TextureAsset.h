#pragma once

#include <string>
#include <DirectXTex.h>

#include "Asset.h"
#include "../Renderer/RHI/Resource.h"
#include "../Renderer/RHI/Descriptor.h"

namespace Warp
{

	struct TextureAsset : Asset
	{
		static constexpr EAssetType StaticType = EAssetType::Texture;

		TextureAsset() : Asset(StaticType) {}

		std::string Filepath;
		DirectX::ScratchImage Image;

		RHITexture Texture;
		RHIDescriptorAllocation SrvAllocation;
		RHIShaderResourceView	Srv;
	};

}