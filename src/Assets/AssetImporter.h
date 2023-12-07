#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>

#include "../Renderer/Renderer.h"
#include "Asset.h"
#include "AssetManager.h"
#include "MeshAsset.h"

namespace Warp
{

	enum class AssetFileExtension
	{
		Unknown,
		Gltf,
	};

	class AssetImporter
	{
	public:
		AssetImporter() = default;
		AssetImporter(Renderer* renderer, AssetManager* assetManager)
			: m_renderer(renderer)
			, m_assetManager(assetManager)
		{
		}

		// If importer does not support the extension - AssetFileExtension::Unknown is returned.
		// Make sure to check for that condition as well
		AssetFileExtension GetFilepathExtension(std::string_view filepath) const
		{
			std::string extension = std::filesystem::path(filepath).extension().string();
			if (m_supportedFileExtensions.find(extension) == m_supportedFileExtensions.end())
			{
				return AssetFileExtension::Unknown;
			}

			return m_supportedFileExtensions.at(extension);
		}

	protected:
		void AddSupportedFormat(const std::string& format, AssetFileExtension extension)
		{
			m_supportedFileExtensions[format] = extension;
		}

		std::unordered_map<std::string, AssetFileExtension> m_supportedFileExtensions;

		Renderer* GetRenderer() { return m_renderer; }
		Renderer* m_renderer = nullptr;

		AssetManager* GetAssetManager() { return m_assetManager; }
		AssetManager* m_assetManager = nullptr;
	};

	class MeshImporter : public AssetImporter
	{
	public:
		MeshImporter() = default;
		MeshImporter(Renderer* renderer, AssetManager* assetManager)
			: AssetImporter(renderer, assetManager)
		{
			AddSupportedFormat(".gltf", AssetFileExtension::Gltf);
		}

		std::vector<AssetProxy> ImportFromFile(std::string_view filepath);

	private:
		void UploadGpuResources(MeshAsset* mesh);
	};

}