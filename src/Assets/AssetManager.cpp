#include "AssetManager.h"

namespace Warp
{

    AssetManager::~AssetManager()
    {
        WARP_LOG_INFO("AssetManager::Destructor -> Destroying asset manager");

        // Iterate over each asset and destroy it here, warning user that there was a memory cleanup needed to shut the manager down
        ResetRegistry<TextureAsset>();
        ResetRegistry<MaterialAsset>();
        ResetRegistry<MeshAsset>();
    }

    AssetProxy AssetManager::DestroyAsset(AssetProxy proxy)
    {
        WARP_ASSERT(proxy.IsValid());

        AssetProxy result;
        switch (proxy.Type)
        {
        case EAssetType::Texture: result = m_textureRegistry.DestroyAsset(proxy); break;
        case EAssetType::Material: result = m_materialRegistry.DestroyAsset(proxy); break;
        case EAssetType::Mesh: result = m_meshRegistry.DestroyAsset(proxy); break;
        case EAssetType::Unknown: WARP_ATTR_FALLTHROUGH;
        default: WARP_ASSERT(false, "Nothing to delete? How come"); return AssetProxy();
        }

        m_proxyTable.erase(proxy.ID);
        return result;
    }

    WARP_ATTR_NODISCARD AssetProxy AssetManager::GetAssetProxy(uint32_t ID)
    {
        if (ID == Asset::InvalidID)
        {
            return AssetProxy();
        }

        auto it = m_proxyTable.find(ID);
        return it == m_proxyTable.end() ? AssetProxy() : it->second;
    }

    WARP_ATTR_NODISCARD AssetProxy AssetManager::GetAssetProxy(const std::string& filepath)
    {
        if (filepath.empty())
        {
            return AssetProxy();
        }

        auto it = m_filepathCache.find(filepath);
        if (it == m_filepathCache.end())
        {
            return AssetProxy();
        }

        AssetProxy proxy = it->second;

        // TODO: Maybe change this all story with filepaths and asset caching?
        // Its not so easy to clear the filepath cache, thats why it sucks... the only way to delete invalid elements is by
        // basically removing them on query... thats a bad way to flush cache
        if (!proxy.IsValid())
        {
            m_filepathCache.erase(filepath);
        }

        return proxy;
    }

}