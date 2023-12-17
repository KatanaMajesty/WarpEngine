#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <type_traits>
#include <concepts>

#include "Asset.h"
#include "MeshAsset.h"
#include "TextureAsset.h"

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
			Registry<T>* registry = this->GetRegistry<T>();
			WARP_ASSERT(registry->IsValid(proxy), "Proxy is invalid!");
			return registry->GetAsset(proxy);
		}

		template<ValidAssetType T>
		bool IsValid(AssetProxy proxy) const
		{
			const Registry<T>* registry = this->GetRegistry<T>();
			return registry->IsValid(proxy);
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
			WARP_ATTR_NODISCARD bool IsValid(AssetProxy proxy) const;

			// Queries an asset that the proxy is pointing into
			T* GetAsset(AssetProxy proxy) const;

			// Resets the registry. You should probably never call this function at this point
			// If you call the function though, ensure that there are NO asset proxies left alive from this registry
			void Reset();

			struct AssetAllocation
			{
				std::unique_ptr<T> Ptr;
				size_t UniqueID;
			};
			std::vector<AssetAllocation> AssetContainer;
			std::queue<size_t> FreedProxies;
			size_t LastUniqueID = 0;
		};

		template<typename T>
		Registry<T>* GetRegistry()
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

		template<typename T>
		const Registry<T>* GetRegistry() const
		{
			using NonConstMe = std::remove_const_t<std::remove_pointer_t<decltype(this)>>*;
			return const_cast<NonConstMe>(this)->GetRegistry<T>();
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
		proxy.UniqueRegistryID = LastUniqueID++;

		if (!FreedProxies.empty())
		{
			proxy.Index = FreedProxies.front();
			FreedProxies.pop();

			WARP_ASSERT(proxy.Index < AssetContainer.size() && !AssetContainer[proxy.Index].Ptr);
		}
		else
		{
			proxy.Index = AssetContainer.size();
			AssetContainer.emplace_back(); // emplace empty allocation that we will fill later
		}

		AssetContainer[proxy.Index] = AssetAllocation{
			.Ptr = std::make_unique<T>(),
			.UniqueID = proxy.UniqueRegistryID
		};

		WARP_ASSERT(proxy.IsValid());
		return proxy;
	}

	template<ValidAssetType T>
	inline AssetProxy AssetManager::Registry<T>::DestroyAsset(AssetProxy proxy)
	{
		// Ensure that the asset for this proxy is still alive
		if (!proxy.IsValid())
		{
			WARP_LOG_WARN("Tried destroying the asset with invalid proxy AssetProxy(Type: {}, Index: {})!", 
				GetAssetTypeName(proxy.Type), proxy.Index);

			return AssetProxy();
		}

		AssetAllocation& allocation = AssetContainer[proxy.Index];

		// reset the ptr and add index to freed indices
		allocation.Ptr.reset();
		FreedProxies.push(proxy.Index);

		return AssetProxy(); // Just return invalid proxy
	}

	template<ValidAssetType T>
	inline bool AssetManager::Registry<T>::IsValid(AssetProxy proxy) const
	{
		if (!proxy.IsValid())
		{
			return false;
		}

		if (proxy.Type != T::StaticType)
		{
			return false;
		}

		// Check if there is ANY asset assosiated with the current proxy
		if (proxy.Index >= AssetContainer.size() || !AssetContainer[proxy.Index].Ptr)
		{
			return false;
		}

		// Now check unique IDs to be the same to ensure we are trying to access the same asset
		const AssetAllocation& allocation = AssetContainer[proxy.Index];
		if (proxy.UniqueRegistryID != allocation.UniqueID)
		{
			return false;
		}

		return true;
	}

	template<ValidAssetType T>
	inline T* AssetManager::Registry<T>::GetAsset(AssetProxy proxy) const
	{
		WARP_EXPAND_DEBUG_ONLY(
			if (!IsValid(proxy))
			{
				WARP_LOG_ERROR("Tried to access asset using invalid proxy AssetProxy(Type: {}, Index: {})!",
					GetAssetTypeName(proxy.Type), proxy.Index);
				WARP_ASSERT(false);

				return nullptr;
			}
		);
		const AssetAllocation& allocation = AssetContainer[proxy.Index];
		return allocation.Ptr.get();
	}

	template<ValidAssetType T>
	inline void AssetManager::Registry<T>::Reset()
	{
		AssetContainer.clear();
		std::queue<size_t>().swap(FreedProxies);

		LastUniqueID = 0;
	}

}