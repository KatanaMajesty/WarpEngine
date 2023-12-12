#pragma once

#include "AssetImporter.h"
#include "TextureAsset.h"

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
		TextureImporter(Renderer* renderer, AssetManager* assetManager)
			: AssetImporter(renderer, assetManager)
		{
			AddSupportedFormat(".bmp", AssetFileExtension::Bmp);
			AddSupportedFormat(".png", AssetFileExtension::Png);
			AddSupportedFormat(".jpeg", AssetFileExtension::Jpeg);
		}

		AssetProxy ImportFromFile(std::string_view filepath, const ImportDesc& importDesc);
		AssetProxy ImportFromImage(const ImageLoader::Image& image);
	};

}