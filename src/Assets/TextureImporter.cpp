#include "TextureImporter.h"

#include <algorithm>

#include "../Core/Logger.h"
#include "../Core/Assert.h"

#include "Util/ImageLoader.h"
#include "../Util/String.h"

namespace Warp
{

	AssetProxy TextureImporter::ImportFromFile(const std::string& filepath, const ImportDesc& importDesc)
	{
		AssetFileExtension extension = GetFilepathExtension(filepath);
		if (extension == AssetFileExtension::Unknown)
		{
			WARP_LOG_ERROR("Failed to load {} as the extension is unsupported", filepath);
			return AssetProxy();
		}

		ImageLoader::Image image;
		switch (extension)
		{
		case AssetFileExtension::Bmp:
		case AssetFileExtension::Png:
		case AssetFileExtension::Jpeg:
			image = ImageLoader::LoadWICFromFile(filepath, importDesc.GenerateMips);
			break;
		default: WARP_ASSERT(false, "Shouldn't happen"); break;
		}

		AssetProxy proxy = ImportFromImage(image);
		return proxy;
	}

	AssetProxy TextureImporter::ImportFromImage(const ImageLoader::Image& image)
	{
		if (!image.IsValid())
		{
			return AssetProxy();
		}

		// Avoid loading a texture multiple times
		if (!image.Filepath.empty())
		{
			auto it = m_textureCache.find(image.Filepath);
			if (it != m_textureCache.end())
			{
				AssetProxy cachedProxy = m_textureCache.at(image.Filepath);
				if (GetAssetManager()->IsValid<TextureAsset>(cachedProxy))
				{
					return cachedProxy;
				}
			}
		}

		AssetManager* assetManager = GetAssetManager();
		AssetProxy proxy = assetManager->CreateAsset<TextureAsset>();
		auto asset = assetManager->GetAs<TextureAsset>(proxy);

		Renderer* renderer = GetRenderer();
		RHIDevice* Device = renderer->GetDevice();

		const DirectX::TexMetadata& metadata = image.DxImage.GetMetadata();
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

		// TODO: We should check if a texture has filepath. If not, give another name
		const std::string& textureName = "ImportedTexture_" + (image.Name.empty() ? image.Filepath : image.Name);
		asset->Texture = RHITexture(Device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, desc);
		asset->Texture.SetName(StringToWString(textureName)); 
		asset->SrvAllocation = Device->GetViewHeap()->Allocate(1);
		asset->Srv = RHIShaderResourceView(Device, &asset->Texture, nullptr, asset->SrvAllocation);

		// Upload an image to an asset's texture
		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		for (size_t i = 0; i < image.DxImage.GetImageCount(); ++i)
		{
			auto subImage = image.DxImage.GetImages()[i];
			subresources.push_back(D3D12_SUBRESOURCE_DATA{
				.pData = subImage.pixels,
				.RowPitch = (LONG_PTR)subImage.rowPitch,
				.SlicePitch = (LONG_PTR)subImage.slicePitch
			});
		}

		RHICopyCommandContext& copyContext = renderer->GetCopyContext();
		copyContext.BeginCopy();
		copyContext.Open();
		{
			copyContext.UploadSubresources(&asset->Texture, subresources, 0);
		}
		copyContext.Close();

		UINT64 fenceValue = copyContext.Execute(false);
		copyContext.EndCopy(fenceValue);

		// Add loaded texture to cache before returning
		if (!image.Filepath.empty())
		{
			m_textureCache[image.Filepath] = proxy;
		}

		return proxy;
	}

}