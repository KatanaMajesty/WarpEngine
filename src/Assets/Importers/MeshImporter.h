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
		AssetProxy ImportStaticMeshFromGltfFile(const std::string& filepath);

		// TODO: Maybe this should be moved elsewhere? Who cares
		struct MeshletComputationAndOptimizationContext
		{
			bool OptimizeSubmesh();

		};

		TextureImporter m_textureImporter;
	};

}