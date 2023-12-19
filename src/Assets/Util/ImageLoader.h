#pragma once

#include "../Asset.h"
#include "../AssetManager.h"
#include "../../Renderer/Renderer.h"

namespace Warp::ImageLoader
{

	struct Image
	{
		bool IsValid() const { return DxImage.GetImages() != nullptr; }

		std::string Name;
		std::string Filepath; // This may be empty, if the Image was loaded from memory
		DirectX::ScratchImage DxImage;
	};

	Image LoadWICFromFile(std::string_view filepath, bool generateMips);
	Image LoadDDSFromFile(std::string_view filepath, bool generateMips);

}