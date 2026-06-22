#pragma once

#include "common/assert.h"
#include "nri_shader_compiler.h"

#include <filesystem>
#include <memory>
#include <span>
#include <string>
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

struct ShaderLibraryConfig
{
    /// A path containing all Warp shaders. Usually should be left at default value preinitialized by Warp engine
    std::filesystem::path shaderPath = std::filesystem::path(".");
    /// Specifies target SPIR-V profile for all shaders compiled by this library.
    ETargetProfile targetProfile = ETargetProfile::Spv_1_6;
    /// Specifies target SPIR-V environment for all shaders compiles by this library.
    ETargetEnvironment targetEnv = ETargetEnvironment::Vk_1_4;
    /// If true every compiled SPIR-V target will be dumped under 'shaderPath' directory
    bool writeSpirvOutputs = false;
};

/// Used by shader library to define shader language of a source file
enum class EShaderLang
{
    Slang,
};

/// @brief Shader library is an object that represents/points to shaders that
/// are stored on system as files. Library also helps cache shaders.
class ShaderLibrary
{
private:
    ShaderLibrary(const ShaderLibraryConfig& config)
        : m_isInitialized(true)
        , m_config(config)
    {
    }

public:
    ShaderLibrary(const ShaderLibrary&) = delete;
    ShaderLibrary& operator=(const ShaderLibrary&) = delete;

    /// Destructor performs a sanity check to see whether this shader library was properly deallocated
    ~ShaderLibrary();

    /// @brief Initializes shader library. Must be called before NRI backend initialization.
    /// Shader library initialization will also set-up shader file cache
    static void Init(const ShaderLibraryConfig& config) noexcept;
    /// This function call MUST NOT be invoked prior to ShaderLibrary::Init or after ShaderLibrary::Deinit
    static ShaderLibrary* Get() noexcept;
    /// @brief Deinitializes shader library object.
    static void Deinit() noexcept;

    /// @brief Compiles a shader with configuration provided to this shader library.
    /// Based on provided language compilation implementation might differ.
    ShaderCompilerOutput CompileFromLibrary(EShaderLang lang,
                                            const std::filesystem::path& shaderName,
                                            std::string_view entryPoint,
                                            std::span<const ShaderPreprocessorMacro> preprocessorMacros) noexcept;

private:
    ShaderCompilerOutput CompileSlangFromLibrary(const std::filesystem::path& shaderName,
                                                 std::string_view entryPoint,
                                                 std::span<const ShaderPreprocessorMacro> preprocessorMacros) noexcept;

    /// Exports resulting SPIR-V output to this shader library. Location of dumped SPIR-V file is defined by
    /// ShaderLibraryConfig. More specifically, dumped SPIR-V binary will be saved at 'ShaderLibraryConfig::shaderPath /
    /// "spirv" / shaderName.spv
    void WriteSpirvOutputToLibrary(const std::filesystem::path& shaderName,
                                   std::string_view entryPoint,
                                   const ShaderCompilerOutput& output);

    static std::unique_ptr<ShaderLibrary> s_instance;

    /// Sanity-check variable to make sure that shader library was properly destroyed!
    bool m_isInitialized = false;
    ShaderLibraryConfig m_config;
    ShaderCompiler m_compiler;
    ShaderLibraryCache m_libraryCache;
};

} // namespace Warp::nri