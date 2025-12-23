#pragma once

#include <string>
#include <filesystem>
#include <unordered_map>

namespace Warp::nri
{

    /// @brief High-level cache that stores recently read shader files at runtime
    class ShaderLibraryCache
    {
    private:
        // Only make it visible to ShaderLibrary itself
        friend class ShaderLibrary;
        ShaderLibraryCache() = default;

        ShaderLibraryCache(const ShaderLibraryCache&) = delete;
        ShaderLibraryCache& operator=(const ShaderLibraryCache&) = delete;

        /// @brief If code was previously stored at filepath updates it with a newly provided value as 'source'.
        inline void Cache(std::string_view filepath, const std::string& source) noexcept
        {
            m_shaderSourceCache[std::string(filepath)] = source;
        }

        /// @brief Checks whether a source was previously cached. If so - returns true, otherwise false
        inline bool IsCached(std::string_view filepath) const noexcept
        {
            return m_shaderSourceCache.contains(std::string(filepath));
        }

    private:
        /// @brief A [path, code] mapping for all previously read shader sources, like Slang or GLSL.
        std::unordered_map<std::string, std::string> m_shaderSourceCache;
    };

    /// @brief Shader library is an object that represents/points to shaders that
    /// are stored on system as files. Library also helps cache shaders.
    class ShaderLibrary
    {
    private:
        ShaderLibrary() = default;

    public:
        ShaderLibrary(const ShaderLibrary&) = delete;
        ShaderLibrary& operator=(const ShaderLibrary&) = delete;

        static ShaderLibrary* Get() noexcept
        {
            static ShaderLibrary instance;
            return &instance;
        }

        /// @brief Initializes shader library. Must be called before NRI backend initialization.
        /// Shader library initialization will also set-up shader file cache
        bool Init() noexcept;

    private:
        ShaderLibraryCache m_libraryCache;
        /// @brief Shader path will only be initialized after ShaderLibrary::Init() is successfully executed
        std::filesystem::path m_shaderPath;
    };

} // Warp::nri namespace