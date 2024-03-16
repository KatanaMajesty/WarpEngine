#pragma once

#include "AssetImporter.h"
#include "TextureImporter.h"

namespace Warp
{

    // TODO: We should provide importer with asset type to import with
    // for example ImportStaticMeshFromFile(const std::string& filepath); -> ImportStaticMeshFromFile(const std::string& filepath, EAssetFormat format);
    // responsibility of determining format of the asset is up to user

    class MeshImporter : public AssetImporter
    {
    public:
        MeshImporter() = default;
        MeshImporter(AssetManager* assetManager)
            : AssetImporter(assetManager)
            , m_textureImporter(assetManager)
        {
            // Add importer's supported formats here
            AddFormat(".gltf", EAssetFormat::Gltf);
        }

        AssetProxy ImportStaticMeshFromFile(const std::string& filepath);

    private:
        AssetProxy ImportStaticMeshFromGltfFile(const std::string& filepath);

        TextureImporter m_textureImporter;
    };

}