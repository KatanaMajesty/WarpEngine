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
		static constexpr uint32_t InvalidID = uint32_t(-1);
		static constexpr EAssetType StaticType = EAssetType::Unknown;

		Asset() = default;
		Asset(uint32_t ID, EAssetType type)
			: m_ID(ID)
			, m_type(type)
			, m_Guid(Guid::Create())
		{
		}

		inline constexpr uint32_t GetID() const { return m_ID; }
		inline constexpr const Guid& GetGuid() const { return m_Guid; }
		inline constexpr EAssetType GetType() const { return m_type; }

		inline constexpr bool IsValid() const { return GetID() != Asset::InvalidID && m_type != EAssetType::Unknown; }

	protected:
		uint32_t m_ID = Asset::InvalidID;
		EAssetType m_type = EAssetType::Unknown;

		Guid m_Guid;
	};

	struct AssetProxy
	{
		inline constexpr bool IsValid() const
		{
			return ID != Asset::InvalidID && 
				Index != Asset::InvalidID && Type != EAssetType::Unknown;
		}

		uint32_t ID = Asset::InvalidID; // Represents a unique identifier of an asset at runtime
		uint32_t Index = Asset::InvalidID; // Index represents a value that can be used to access the actual asset inside of the registry
		EAssetType Type = EAssetType::Unknown;
	};

}