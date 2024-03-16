#pragma once

#include "AssetImporter.h"

namespace Warp
{

    struct TextureImportDesc
    {
        bool GenerateMips = false;
    };

    class TextureImporter : public AssetImporter
    {
    public:
        TextureImporter() = default;
        TextureImporter(AssetManager* assetManager)
            : AssetImporter(assetManager)
        {
            // Add importer's supported formats here
            AddFormat(".bmp", EAssetFormat::Bmp);
            AddFormat(".png", EAssetFormat::Png);
            AddFormat(".jpg", EAssetFormat::Jpeg);
            AddFormat(".jpeg", EAssetFormat::Jpeg);
        }

        AssetProxy ImportFromFile(const std::string& filepath, const TextureImportDesc& importDesc);
    };

}