#include "ImageLoader.h"

#include <cstring>
#include <cstdint>
#include <filesystem>

#include "../../Util/String.h"
#include "../../Core/Logger.h"
#include "../../Core/Assert.h"
#include "../TextureAsset.h"

namespace Warp::ImageLoader
{

	Image LoadWICFromFile(std::string_view filepath, bool generateMips)
	{
		using namespace DirectX;

		std::wstring wFilepath = StringToWString(filepath);
		ScratchImage image;
		HRESULT hr = LoadFromWICFile(wFilepath.c_str(), WIC_FLAGS_NONE, nullptr, image);
		if (FAILED(hr))
		{
			WARP_LOG_ERROR("Failed to load image from {}", filepath);
			return Image();
		}

		if (generateMips)
		{
			ScratchImage mipChain;
			hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), TEX_FILTER_DEFAULT, 0, mipChain);
			if (FAILED(hr))
			{
				WARP_LOG_ERROR("Failed to generate mip levels for {}", filepath);
				return Image();
			}

			image = std::move(mipChain);
		}
		
		return Image{
			.Filepath = std::string(filepath),
			.DxImage = std::move(image)
		};
	}

	Image LoadDDSFromFile(std::string_view filepath, bool generateMips)
	{
		WARP_YIELD_NOIMPL();
		return Image();
	}

}