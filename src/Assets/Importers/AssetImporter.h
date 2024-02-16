#pragma once

#include <map>
#include "../Asset.h"

namespace Warp
{

	enum class EAssetFormat
	{
		Unknown,
		Gltf,
		Bmp,
		Png,
		Jpeg,
	};

	const char* FileExtentionFromFormat(EAssetFormat format);

	class AssetManager;

	class AssetImporter
	{
	public:
		AssetImporter() = default;
		AssetImporter(AssetManager* assetManager)
			: m_assetManager(assetManager)
		{
		}

		// Returns a valid asset format if an importer is aware of how to process this file format
		// Otherwise, returns EAssetFormat::Unknown
		inline EAssetFormat GetFormat(const std::string& extension)
		{
			auto it = m_supportedFormats.find(extension);
			return it == m_supportedFormats.end() ? EAssetFormat::Unknown : it->second;
		}

		inline AssetManager* GetAssetManager() const { return m_assetManager; }

	protected:
		inline void AddFormat(const std::string& extension, EAssetFormat format)
		{
			m_supportedFormats.emplace(extension, format);
		}

		std::map<std::string, EAssetFormat> m_supportedFormats;
		AssetManager* m_assetManager = nullptr;
	};

}