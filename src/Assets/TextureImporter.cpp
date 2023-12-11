#include "TextureImporter.h"

#include "../Core/Logger.h"
#include "../Core/Assert.h"

#include "Util/ImageLoader.h"

namespace Warp
{

	AssetProxy TextureImporter::ImportFromFile(std::string_view filepath, const ImportDesc& importDesc)
	{
		AssetFileExtension extension = GetFilepathExtension(filepath);
		if (extension == AssetFileExtension::Unknown)
		{
			WARP_LOG_ERROR("Failed to load {} as the extension is unsupported", filepath);
			return AssetProxy();
		}

		AssetManager* assetManager = GetAssetManager();
		AssetProxy proxy = AssetProxy();
		switch (extension)
		{
		case AssetFileExtension::Bmp:
		case AssetFileExtension::Png:
		case AssetFileExtension::Jpeg:
			proxy = ImageLoader::LoadWICFromFile(filepath, assetManager, importDesc.GenerateMips); 
			break;
		default: WARP_ASSERT(false, "Shouldn't happen"); break;
		}

		if (!proxy.IsValid())
		{
			return proxy;
		}

		auto asset = assetManager->GetAs<TextureAsset>(proxy);
		
		Renderer* renderer = GetRenderer();
		RHIDevice* Device = renderer->GetDevice();

		const DirectX::TexMetadata& metadata = asset->Image.GetMetadata();
		D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC();
		switch (metadata.dimension)
		{
		case DirectX::TEX_DIMENSION_TEXTURE1D: desc = CD3DX12_RESOURCE_DESC::Tex1D(metadata.format, metadata.width, 
			metadata.arraySize, 
			metadata.mipLevels, 
			(D3D12_RESOURCE_FLAGS)metadata.miscFlags); break;
		case DirectX::TEX_DIMENSION_TEXTURE2D: desc = CD3DX12_RESOURCE_DESC::Tex2D(metadata.format, metadata.width, metadata.height, 
			metadata.arraySize, 
			metadata.mipLevels, 1U, 0U, 
			(D3D12_RESOURCE_FLAGS)metadata.miscFlags); break;
		case DirectX::TEX_DIMENSION_TEXTURE3D: desc = CD3DX12_RESOURCE_DESC::Tex3D(metadata.format, metadata.width, metadata.height, 
			metadata.depth,
			metadata.mipLevels,
			(D3D12_RESOURCE_FLAGS)metadata.miscFlags); break;
		default: WARP_ASSERT(false); break;
		}
		
		asset->Texture = RHITexture(Device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, desc);
		asset->SrvAllocation = Device->GetViewHeap()->Allocate(1);
		asset->Srv = RHIShaderResourceView(Device, &asset->Texture, nullptr, asset->SrvAllocation);

		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		for (size_t i = 0; i < asset->Image.GetImageCount(); ++i)
		{
			auto image = asset->Image.GetImages()[i];
			subresources.push_back(D3D12_SUBRESOURCE_DATA{
				.pData = image.pixels,
				.RowPitch = (LONG_PTR)image.rowPitch,
				.SlicePitch = (LONG_PTR)image.slicePitch
				});
		}

		renderer->UploadSubresources(&asset->Texture, subresources, 0);

		// Release an image after we're done importing
		asset->Image.Release();

		return proxy;
	}

}