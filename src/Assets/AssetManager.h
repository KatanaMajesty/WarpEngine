#pragma once

#include <vector>
#include <queue>
#include <unordered_map>
#include <string>
#include <memory>
#include <type_traits>
#include <concepts>

#include "Asset.h"
#include "MaterialAsset.h"
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
		//
		// (14.02.2024) -> CreateAsset() now generates Guid object while creating asset. Guid is a unique identifier of an asset
		// Guid-to-Proxy mapping is then added to AssetManager's proxy map
		template<ValidAssetType T>
		WARP_ATTR_NODISCARD AssetProxy CreateAsset()
		{
			Registry<T>* registry = this->GetRegistry<T>();

			Guid ID = Guid::Create();
			AssetProxy proxy = registry->AllocateAsset(ID);
			if (registry->IsValid(proxy))
			{
				m_proxyTable[ID] = proxy;
			}

			return proxy;
		}

		// Allocates an asset and returns a proxy to an asset
		// Basically does the same behavior as CreateAsset() although it will also associate a provided filepath (or basically any unique name)
		// with the allocated asset
		template<ValidAssetType T>
		WARP_ATTR_NODISCARD AssetProxy CreateAsset(const std::string& filepath)
		{
			AssetProxy proxy = GetAssetProxy(filepath);

			// If found cached proxy in manager's cache tables
			if (proxy.IsValid())
			{
				if (proxy.Type != T::StaticType)
				{
					WARP_LOG_ERROR("Wrong type is requested for the filepath. Perhaps internal resource?");
					WARP_ASSERT(false);
					return AssetProxy();
				}

				// Early return cached proxy
				return proxy;
			}

			proxy = CreateAsset<T>();
			WARP_ASSERT(proxy.IsValid());

			m_filepathCache[filepath] = proxy.ID;
			return proxy;
		}

		// Destroys an asset with the associated proxy
		// returns an updated proxy (empty or invalid proxy)
		WARP_ATTR_NODISCARD AssetProxy DestroyAsset(AssetProxy proxy);

		// Tries to get a proxy for the associated asset's unique ID. This may be usefull to check if an asset is indeed associated with this manager
		// Returns empty asset if there is no proxy, thus no asset, associated with the provided ID parameter
		WARP_ATTR_NODISCARD AssetProxy GetAssetProxy(const Guid& ID);

		// Tries to find an asset proxy by filepath (or whatever unique name you want basically) using two levels of indirection
		// Firstly, manager will search for a Guid associated with filepath provided
		// Secondly, if any Guid was found, searches for asset proxy in the proxy table for the Guid found
		// Returns valid asset proxy if successfully found associated asset, otherwise returns invalid proxy
		//
		// (14.02.2024) -> Maybe two levels of indirection via Guid are obsolete and may be replaced by one?
		WARP_ATTR_NODISCARD AssetProxy GetAssetProxy(const std::string& filepath);

		// A very quick search of an asset (linear O(1) essentially, just an array lookup). As of 31/12/23 performs checks on whether the asset proxy is valid
		// And if the proxy is not valid - nullptr is returned
		// In debug configuration only assertions are performed to check whether to proxy is valid and can be used to retrieve an asset
		template<ValidAssetType T>
		T* GetAs(AssetProxy proxy)
		{
			Registry<T>* registry = this->GetRegistry<T>();
			return registry->IsValid(proxy) ? registry->GetAsset(proxy) : nullptr;
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

			WARP_ATTR_NODISCARD AssetProxy AllocateAsset(const Guid& ID);
			WARP_ATTR_NODISCARD AssetProxy DestroyAsset(AssetProxy proxy);
			WARP_ATTR_NODISCARD bool IsValid(AssetProxy proxy) const;

			// Queries an asset that the proxy is pointing into
			T* GetAsset(AssetProxy proxy) const;

			// Resets the registry. You should probably never call this function at this point
			// If you call the function though, ensure that there are NO asset proxies left alive from this registry
			void Reset();

			// This is how we store assets... insane, right? Allocation? Lmao it is indeed an ALLOCATION
			struct AssetAllocation
			{
				std::unique_ptr<T> Ptr;
			};
			std::vector<AssetAllocation> AssetContainer;
			std::queue<uint32_t> FreedProxies;
		};

		template<typename T>
		Registry<T>* GetRegistry()
		{
			if constexpr (std::is_same_v<T, MaterialAsset>)
			{
				return &m_materialRegistry;
			}
			else if constexpr (std::is_same_v<T, MeshAsset>)
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

		Registry<MaterialAsset> m_materialRegistry;
		Registry<MeshAsset> m_meshRegistry;
		Registry<TextureAsset> m_textureRegistry;
		std::unordered_map<Guid, AssetProxy> m_proxyTable;
		std::unordered_map<std::string, Guid> m_filepathCache;
	};

	template<ValidAssetType T>
	inline AssetProxy AssetManager::Registry<T>::AllocateAsset(const Guid& ID)
	{
		AssetProxy proxy = AssetProxy();
		proxy.Type = T::StaticType;
		proxy.ID = ID;

		if (!FreedProxies.empty())
		{
			proxy.Index = FreedProxies.front();
			FreedProxies.pop();

			WARP_ASSERT(proxy.Index < AssetContainer.size() && !AssetContainer[proxy.Index].Ptr);
		}
		else
		{
			proxy.Index = static_cast<uint32_t>(AssetContainer.size()); // Cast is safe for now (and probs forever)
			AssetContainer.emplace_back(); // emplace empty allocation that we will fill later
		}

		// Assume that valid asset would always take in uint32_t ID as a single constructor parameter
		AssetContainer[proxy.Index] = AssetAllocation{
			.Ptr = std::make_unique<T>(ID),
		};

		WARP_ASSERT(proxy.IsValid());
		return proxy;
	}

	template<ValidAssetType T>
	inline AssetProxy AssetManager::Registry<T>::DestroyAsset(AssetProxy proxy)
	{
		// Ensure that the asset for this proxy is still alive
		if (!IsValid(proxy))
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
		if (proxy.ID != allocation.Ptr->GetID())
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
	}

}