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

        TextureAsset(uint32_t ID) : Asset(ID, StaticType) {}

        RHITexture Texture;
        RHIDescriptorAllocation SrvAllocation;
        RHIShaderResourceView Srv;
    };

}