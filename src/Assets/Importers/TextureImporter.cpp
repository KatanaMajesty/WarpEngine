#include "TextureImporter.h"

#include <algorithm>
#include <filesystem>

#include "../../Core/Application.h"
#include "../../Util/Logger.h"
#include "../../Core/Assert.h"

#include "../../Renderer/Renderer.h"
#include "../../Util/String.h"

#include "Formats/ImageLoader.h"
#include "../AssetManager.h"

#include "../../WinWrap.h"

namespace Warp
{

    AssetProxy TextureImporter::ImportFromFile(const std::string& filepath, const TextureImportDesc& importDesc)
    {
        // 17.04.24 -> Check if COM library is available at runtime. If not - bail out and yell
        // TODO: Bad decision to do such checks. You never know when you want to support other platforms
        if (!WinWrap::ScopedCOMLibrary::IsInitialized())
        {
            WARP_LOG_ERROR("TextureImporter::ImportFromFile -> COM Library is not initialized!");
            return AssetProxy();
        }

        EAssetFormat format = GetFormat(std::filesystem::path(filepath).extension().string());
        if (format == EAssetFormat::Unknown)
        {
            WARP_LOG_ERROR("Failed to load {} as the extension is unsupported", filepath);
            return AssetProxy();
        }

        AssetManager* manager = GetAssetManager();
        AssetProxy proxy = manager->GetAssetProxy(filepath);
        if (proxy.IsValid())
        {
            WARP_ASSERT(proxy.Type == EAssetType::Texture, "This should only be texture! Nothing else");
            WARP_LOG_INFO("TextureImporter::ImportFromFile -> Returning cached asset proxy for a texture \'{}\'", filepath);
            return proxy;
        }

        ImageLoader::Image image;
        switch (format)
        {
        case EAssetFormat::Bmp:
        case EAssetFormat::Png:
        case EAssetFormat::Jpeg:
            image = ImageLoader::LoadWICFromFile(filepath, importDesc.GenerateMips);
            break;
        default: WARP_ASSERT(false, "Shouldn't happen"); break;
        }

        if (!image.IsValid())
        {
            WARP_LOG_ERROR("TextureImporter::ImportFromFile -> Failed to load image from file \'{}\'", filepath);
            return AssetProxy();
        }

        proxy = manager->CreateAsset<TextureAsset>(filepath);
        TextureAsset* asset = manager->GetAs<TextureAsset>(proxy);

        // TODO: (14.02.2024) -> Singleton... meh
        Renderer* renderer = Application::Get().GetRenderer();
        RHIDevice* Device = renderer->GetDevice();

        const DirectX::TexMetadata& metadata = image.DxImage.GetMetadata();
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC();
        switch (metadata.dimension)
        {
        case DirectX::TEX_DIMENSION_TEXTURE1D: desc = CD3DX12_RESOURCE_DESC::Tex1D(metadata.format, metadata.width,
            static_cast<UINT16>(metadata.arraySize),
            static_cast<UINT16>(metadata.mipLevels),
            (D3D12_RESOURCE_FLAGS)metadata.miscFlags); break;
        case DirectX::TEX_DIMENSION_TEXTURE2D: desc = CD3DX12_RESOURCE_DESC::Tex2D(metadata.format, metadata.width, static_cast<UINT>(metadata.height),
            static_cast<UINT16>(metadata.arraySize),
            static_cast<UINT16>(metadata.mipLevels), 1U, 0U,
            (D3D12_RESOURCE_FLAGS)metadata.miscFlags); break;
        case DirectX::TEX_DIMENSION_TEXTURE3D: desc = CD3DX12_RESOURCE_DESC::Tex3D(metadata.format, metadata.width, static_cast<UINT>(metadata.height),
            static_cast<UINT16>(metadata.depth),
            static_cast<UINT16>(metadata.mipLevels),
            (D3D12_RESOURCE_FLAGS)metadata.miscFlags); break;
        default: WARP_ASSERT(false); break;
        }

        asset->Texture = RHITexture(Device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, desc);
        if (!image.Filepath.empty())
        {
            asset->Texture.SetName(StringToWString(image.Filepath));
        }

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

        return proxy;
    }

}