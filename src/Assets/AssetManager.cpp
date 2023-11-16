#include "AssetManager.h"

namespace Warp
{

	AssetProxy AssetManager::DestroyAsset(AssetProxy proxy)
	{
		WARP_ASSERT(proxy.IsValid());
		switch (proxy.Type)
		{
		case EAssetType::Model: return m_modelRegistry.DestroyAsset(proxy);
		case EAssetType::Texture: return m_textureRegistry.DestroyAsset(proxy);
		case EAssetType::Unknown: WARP_ATTR_FALLTHROUGH;
		default: WARP_ASSERT(false, "Nothing to delete? How come"); return AssetProxy();
		}
	}

}