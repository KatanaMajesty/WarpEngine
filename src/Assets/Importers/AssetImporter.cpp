#include "AssetImporter.h"

#include "../../Core/Defines.h"
#include "../../Core/Assert.h"

namespace Warp
{

    const char* FileExtentionFromFormat(EAssetFormat format)
    {
        switch (format)
        {
        case EAssetFormat::Gltf: return ".gltf";
        case EAssetFormat::Bmp: return ".bmp";
        case EAssetFormat::Png: return ".png";
        case EAssetFormat::Jpeg: return ".jpeg";
        case EAssetFormat::Unknown: WARP_ATTR_FALLTHROUGH;
        default: WARP_ASSERT(false); return "";
        }
    }

}