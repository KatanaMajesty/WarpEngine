#pragma once

#include <unordered_map>

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

		AssetProxy ImportFromFile(const std::string& filepath, const ImportDesc& importDesc);
		AssetProxy ImportFromImage(const ImageLoader::Image& image);

	private:
		// TODO: Cache only works with filepaths... Maybe use more metadata information to store it?
		std::unordered_map<std::string, AssetProxy> m_textureCache;
	};

}