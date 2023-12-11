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

	//AssetProxy LoadFromFile(std::string_view filepath, Renderer* renderer, AssetManager* assetManager)
	//{
	//	// TODO: see HDR discussion in stb_image.h

	//	const char* pFilepath = filepath.data();
	//	int32_t width;
	//	int32_t height;
	//	int32_t reqChannels;
	//	int32_t r = stbi_info(pFilepath, &width, &height, &reqChannels);
	//	if (r != 1)
	//	{
	//		WARP_LOG_ERROR("Unsupported format of image: {}", pFilepath);
	//		return AssetProxy();
	//	}

	//	// We want to use 4 channels if the texture is 3-channels only
	//	// thus RGB image -> RGBA texture
	//	if (reqChannels == 3)
	//		reqChannels == 4;

	//	int32_t numChannels;
	//	uint8_t* data = stbi_load(pFilepath, &width, &height, &numChannels, reqChannels);

	//	if (!data)
	//	{
	//		WARP_LOG_ERROR("Failed to load {}", pFilepath);
	//		return AssetProxy();
	//	}

	//	WARP_ASSERT(width > 0 && height > 0);
	//	WARP_ASSERT(numChannels > 0);

	//	AssetProxy proxy = assetManager->CreateAsset<TextureAsset>();
	//	auto asset = assetManager->GetAs<TextureAsset>(proxy);
	//	
	//	RHIDevice* Device = renderer->GetDevice();

	//	// TODO: Figure out the most precise way to determine format of an image
	//	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	//	switch (numChannels)
	//	{
	//	case 1: format = DXGI_FORMAT_R8_UNORM; break; // grey
	//	case 2: format = DXGI_FORMAT_R8G8_UNORM; break; // grey, alpha
	//	case 4: format = DXGI_FORMAT_R8G8B8A8_UNORM; break; // red, green, blue, opt. alpha
	//	default: WARP_ASSERT(false, "Unsupported num of channels?"); break;
	//	}

	//	D3D12_RESOURCE_DESC desc;
	//	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	//	desc.Alignment = 0;
	//	desc.Width = static_cast<UINT64>(width);
	//	desc.Height = static_cast<UINT>(height);
	//	desc.DepthOrArraySize = 1;
	//	desc.MipLevels = 0; // Calculate mips automatically
	//	desc.Format = format;
	//	desc.SampleDesc.Count = 1;
	//	desc.SampleDesc.Quality = 0;
	//	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	//	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	//	FLOAT clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	//	RHITexture uploadTexture = RHITexture(Device,
	//		D3D12_HEAP_TYPE_UPLOAD,
	//		D3D12_RESOURCE_STATE_COMMON, desc, 
	//		CD3DX12_CLEAR_VALUE(format, clearColor));

	//	memcpy(uploadTexture.GetCpu)

	//	stbi_image_free(data);
	//	return AssetProxy();
	//}

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