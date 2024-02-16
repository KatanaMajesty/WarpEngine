#pragma once

#include <string_view>

#include "../Core/Defines.h"
#include "../Util/Guid.h"

namespace Warp
{

	enum class EAssetType
	{
		Unknown = 0,
		Texture,
		Material,
		Mesh,
		NumTypes,
	};

	inline constexpr std::string_view GetAssetTypeName(EAssetType type)
	{
		switch (type)
		{
		case EAssetType::Mesh: return "Mesh";
		case EAssetType::Texture:  return "Texture";
		case EAssetType::Material: return "Material";
		case EAssetType::Unknown: WARP_ATTR_FALLTHROUGH;
		default: return "Unknown";
		}
	}

	class Asset
	{
	public:
		static constexpr uint32_t InvalidIndex = uint32_t(-1);
		static constexpr EAssetType StaticType = EAssetType::Unknown;

		Asset() = default;
		Asset(const Guid& ID, EAssetType type)
			: m_ID(ID)
			, m_type(type)
		{
		}

		inline constexpr const Guid& GetID() const { return m_ID; }
		inline constexpr EAssetType GetType() const { return m_type; }

		inline constexpr bool IsValid() const { return GetID().IsValid() && m_type != EAssetType::Unknown; }

	protected:
		Guid m_ID;
		EAssetType m_type = EAssetType::Unknown;
	};

	struct AssetProxy
	{
		inline constexpr bool IsValid() const
		{
			return Type != EAssetType::Unknown && Index != Asset::InvalidIndex && ID.IsValid();
		}

		Guid ID; // Represents a "unique identifier" of the asset
		uint32_t Index = Asset::InvalidIndex; // Index represents a value that can be used to access the actual asset inside of the registry
		EAssetType Type = EAssetType::Unknown;
	};

}