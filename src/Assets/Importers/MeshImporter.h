#pragma once

#include "AssetImporter.h"
#include "TextureImporter.h"

namespace Warp
{
	
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
		TextureImporter m_textureImporter;
	};

}