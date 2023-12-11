#pragma once

#include "../Asset.h"
#include "../AssetManager.h"
#include "../../Renderer/Renderer.h"

namespace Warp::ImageLoader
{

	struct Image
	{
		std::string Filepath;
		DirectX::ScratchImage DxImage;
	};

	enum class ImageExtension
	{
		Bmp,
		Png,
		Jpeg,
		Dds,
	};

	Image LoadWICFromFile(std::string_view filepath, bool generateMips);
	Image LoadDDSFromFile(std::string_view filepath, bool generateMips);

}