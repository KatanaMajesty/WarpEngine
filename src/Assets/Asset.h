#pragma once

#include <string_view>

#include "../Core/Defines.h"

namespace Warp
{

	enum class EAssetType
	{
		Unknown = 0,
		Mesh,
		Texture,
		NumTypes,
		// Scene,
	};

	struct AssetProxy
	{
		inline constexpr bool IsValid() const
		{
			return Type != EAssetType::Unknown && Index != size_t(-1);
		}

		EAssetType Type = EAssetType::Unknown;
		size_t Index = size_t(-1); // Index represents a value that can be used to access the actual asset inside of the registry
	};

	class Asset
	{
	public:
		static constexpr EAssetType StaticType = EAssetType::Unknown;

		Asset() = default;
		Asset(EAssetType type)
			: m_type(type)
		{
		}

		inline constexpr EAssetType GetType() const { return m_type; }

	protected:
		EAssetType m_type;
	};

}