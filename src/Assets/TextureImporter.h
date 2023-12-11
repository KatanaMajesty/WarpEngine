#pragma once

#include "AssetImporter.h"
#include "TextureAsset.h"

namespace Warp
{

	struct ImportDesc
	{
		bool GenerateMips = false;
	};

	// TODO: We need to introduce a few major changes here. Currently our Assets/Util folders load data into the Asset directly.
	// This is bad as we just waste memory to store maybe unused or maybe obsolete data, that is the one that we use to upload/process Asset during importing
	// We want instead to retrieve generic data from Assets/Util which will then be converted to our Asset types

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
	};

}