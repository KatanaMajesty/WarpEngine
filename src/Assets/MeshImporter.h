#pragma once

#include <vector>

#include "AssetImporter.h"
#include "MeshAsset.h"

namespace Warp
{
	
	// TODO: We need to introduce a few major changes here. Currently our Assets/Util folders load data into the Asset directly.
	// This is bad as we just waste memory to store maybe unused or maybe obsolete data, that is the one that we use to upload/process Asset during importing
	// We want instead to retrieve generic data from Assets/Util which will then be converted to our Asset types

	class MeshImporter : public AssetImporter
	{
	public:
		MeshImporter() = default;
		MeshImporter(Renderer* renderer, AssetManager* assetManager)
			: AssetImporter(renderer, assetManager)
		{
			AddSupportedFormat(".gltf", AssetFileExtension::Gltf);
		}

		std::vector<AssetProxy> ImportFromFile(std::string_view filepath);

	private:
		void UploadGpuResources(MeshAsset* mesh);
	};

}