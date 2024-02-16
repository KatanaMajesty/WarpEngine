#pragma once

#include "AssetImporter.h"

namespace Warp
{

	namespace ImageLoader
	{
		struct Image;
	}

	struct ImportDesc
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
			AddFormat(".jpeg", EAssetFormat::Jpeg);
		}

		AssetProxy ImportFromFile(const std::string& filepath, const ImportDesc& importDesc);
		AssetProxy ImportFromMemory(const ImageLoader::Image& image);
	};

}