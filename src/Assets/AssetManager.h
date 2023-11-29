#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <type_traits>
#include <concepts>

#include "Asset.h"
#include "MeshAsset.h"

#include "../Core/Assert.h"
#include "../Core/Logger.h"

namespace Warp
{

	template<typename T>
	concept ValidAssetType = std::derived_from<T, Asset> && requires(T t) { t.StaticType; t.StaticType != EAssetType::Unknown; };

	// The idea of AssetManager is to create assets and store them inside itself.
	// User is still able to access those assets using an interface-object called AssetProxy. It is a very light-weight object, somewhat simillar to Entity
	// It can be copied and moved around without worrying about efficieny cost
	class AssetManager
	{
	public:
		// Allocates an asset and returns a proxy to an asset
		// The proxy should be stored and can be passed in-and-out in functions as it is a light-weight object
		// Assets are queried from AssetManager on the fly using AssetManager::GetAs member function
		template<ValidAssetType T>
		WARP_ATTR_NODISCARD AssetProxy CreateAsset()
		{
			Registry<T>* registry = this->GetRegistry<T>();
			AssetProxy proxy = registry->AllocateAsset();
			return proxy;
		}

		// Destroys an asset with the associated proxy
		// returns an updated proxy, that will 
		WARP_ATTR_NODISCARD AssetProxy DestroyAsset(AssetProxy proxy);

		// A very quick search of an asset (linear O(1) essentially, just an array lookup). In release configuration performs no checks on whether the asset proxy is valid
		// In debug configuration only assertions are performed to check whether to proxy is valid and can be used to retrieve an asset
		template<ValidAssetType T>
		T* GetAs(AssetProxy proxy)
		{
			WARP_ASSERT(proxy.IsValid(), "Proxy is invalid!")
			WARP_ASSERT(proxy.Type == T::StaticType, "Proxies' asset type differs from the requested");
			Registry<T>* registry = this->GetRegistry<T>();
			return registry->GetAsset(proxy);
		}

	private:
		// Asset registry should not delete asset handles when they are destroyed,
		// but instead should free the place for the asset handles that will be created later

		// TODO: Need a way to retrieve AssetProxy after it was created smhw.
		// TODO: Rewrite registry to use memorypools instead of vectors
		template<ValidAssetType T>
		class Registry
		{
		public:
			Registry() = default;
			explicit Registry(uint32_t numAssets)
			{
				AssetContainer.reserve(numAssets);
			}

			WARP_ATTR_NODISCARD AssetProxy AllocateAsset();
			WARP_ATTR_NODISCARD AssetProxy DestroyAsset(AssetProxy proxy);

			// Queries an asset that the proxy is pointing into
			T* GetAsset(AssetProxy proxy) const;

			void Reset(); // Resets the registry

			std::vector<std::unique_ptr<T>> AssetContainer;
			std::queue<size_t> FreedProxies;
		};

		template<typename T>
		auto GetRegistry() -> Registry<T>*
		{
			if constexpr (std::is_same_v<T, MeshAsset>)
			{
				return &m_meshRegistry;
			}
			else if constexpr (std::is_same_v<T, TextureAsset>)
			{
				return &m_textureRegistry;
			}
			else return nullptr;
		}

		Registry<MeshAsset> m_meshRegistry;
		Registry<TextureAsset> m_textureRegistry;
	};

	// TODO: Maybe move this elsewhere?
	inline constexpr std::string_view GetAssetTypeName(EAssetType type)
	{
		switch (type)
		{
		case EAssetType::Mesh:		return "Mesh";
		case EAssetType::Texture:	return "Texture";
		case EAssetType::Unknown: WARP_ATTR_FALLTHROUGH;
		default: return "Unknown";
		}
	}

	template<ValidAssetType T>
	inline AssetProxy AssetManager::Registry<T>::AllocateAsset()
	{
		AssetProxy proxy = AssetProxy();
		proxy.Type = T::StaticType;

		if (!FreedProxies.empty())
		{
			proxy.Index = FreedProxies.front();
			FreedProxies.pop();

			WARP_ASSERT(proxy.Index < AssetContainer.size() && !AssetContainer[proxy.Index]);
		}
		else
		{
			proxy.Index = AssetContainer.size();
			AssetContainer.emplace_back(); // emplace empty ptr
		}

		AssetContainer[proxy.Index] = std::make_unique<T>();

		WARP_ASSERT(proxy.IsValid());
		return proxy;
	}

	template<ValidAssetType T>
	inline AssetProxy AssetManager::Registry<T>::DestroyAsset(AssetProxy proxy)
	{
		if (!proxy.IsValid())
		{
			WARP_LOG_WARN("Tried destroying the asset with invalid proxy AssetProxy(Type: {}, Index: {})!", 
				GetAssetTypeName(proxy.Type), proxy.Index);

			return AssetProxy();
		}

		// Ensure that the asset for this proxy is still alive
		WARP_ASSERT(proxy.Index < AssetContainer.size() && AssetContainer[proxy.Index]);

		// reset the ptr and add index to freed indices
		AssetContainer[proxy.Index].reset();
		FreedProxies.push(proxy.Index);

		return AssetProxy(); // Just return invalid proxy
	}

	template<ValidAssetType T>
	inline T* AssetManager::Registry<T>::GetAsset(AssetProxy proxy) const
	{
		WARP_EXPAND_DEBUG_ONLY(
			if (!proxy.IsValid() || proxy.Index >= AssetContainer.size() || !AssetContainer[proxy.Index])
			{
				WARP_LOG_ERROR("Tried to access asset using invalid proxy AssetProxy(Type: {}, Index: {})!",
					GetAssetTypeName(proxy.Type), proxy.Index);
				WARP_ASSERT(false);

				return nullptr;
			}
		);
		return AssetContainer[proxy.Index].get();
	}

	template<ValidAssetType T>
	inline void AssetManager::Registry<T>::Reset()
	{
		AssetContainer.clear();
		std::queue<size_t>().swap(FreedProxies);
	}

}