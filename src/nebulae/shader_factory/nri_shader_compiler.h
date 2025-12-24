#pragma once

#include <slang/slang.h>
#include <slang/slang-com-ptr.h>

#include <span>
#include <string_view>
#include <vector>
#include <filesystem>

namespace Warp::nri
{

    /// This is Slang compiler API implementation
    /// For reference see https://shader-slang.org/slang/user-guide/compiling.html#using-the-compilation-api
    /// For further reference on SPIR-V compilation see https://docs.shader-slang.org/en/latest/compilation-api.html
    /// Sascha Williems Slang Vulkan examples: https://www.saschawillems.de/blog/2025/06/03/shaders-for-vulkan-samples-now-also-available-in-slang/
    /// On HLSL-to-SPIRV compilation see https://docs.vulkan.org/guide/latest/hlsl.html

    struct ShaderPreprocessorMacro
    {
        std::string_view name;
        std::string_view value;
    };

    enum class ETargetProfile
    {
        Unknown,
        Spv_1_5,
        Spv_1_6,
    };

    /// Certain target environments impose additional restrictions on SPIR-V, so it's
    /// often necessary to specify which one applies.
    /// This enumeration is derived from spv_target_env, but only includes Vulkan-specific environments, as we only care about those
    enum class ETargetEnvironment
    {
        // If specified will target universal env based of ETargetProfile specified in shader compiler info.
        // For example ETargetEnvironment::Universal and ETargetProfile::Spv_1_5 will result in SPV_ENV_UNIVERSAL_1_5
        Universal,
        Vk_1_0, // Vulkan 1.0 latest revision.
        Vk_1_1, // Vulkan 1.1 latest revision.
        Vk_1_2, // Vulkan 1.2 latest revision.
        Vk_1_3, // Vulkan 1.3 latest revision.
        Vk_1_4, // Vulkan 1.4 latest revision.
    };

    struct ShaderCompilerSlangInfo
    {
        /// @brief A source code of Slang shader (read by the client)
        std::string_view moduleCode;
        /// @brief An arbitrary client-specified module name for this Slang compilation
        std::string_view moduleName;
        /// @brief An entrypoint to use when compiling
        std::string_view entryPoint;
        /// @brief Target profile for SPIR-V output
        ETargetProfile targetProfile = ETargetProfile::Unknown;
        /// @brief An array of include directories to be used for #include directive lookup
        std::span<const std::filesystem::path> includeDirectories;
        /// @brief Any additional preprocessor definitions to be added to the Slang module during compilation
        std::span<const ShaderPreprocessorMacro> preprocessorMacros;
    };

    struct ShaderCompilerValidationInfo
    {
#if defined(WARP_ENGINE_DEBUG)
        /// @brief If set to true every compilation will run with ValidateCompilerSpirvOutput call
        bool enableValidation = true;
#else // !defined(WARP_ENGINE_DEBUG)
        /// @brief If set to true every compilation will run with ValidateCompilerSpirvOutput call
        bool enableValidation = false;
#endif // defined(WARP_ENGINE_DEBUG)
        ETargetEnvironment validationEnvironment = ETargetEnvironment::Universal;
    };

    struct ShaderCompilerOutput
    {
        /// An array of words describing compiled binary SPIR-V module output
        std::vector<uint32_t> spirvWords;
    };

    /// @brief This is a SPIR-V compiler.
    /// TODO: this tool might need to become a standalone project for offline-packaging
    class ShaderCompiler
    {
    public:
        ShaderCompiler() = default;

        ShaderCompiler(const ShaderCompiler&) = delete;
        ShaderCompiler& operator=(const ShaderCompiler&) = delete;

        /// @brief Initializes Slang context and global session.
        bool Init() noexcept;

        /// @brief Compiles Slang to SPIR-V module
        ShaderCompilerOutput CompileSlang(const ShaderCompilerSlangInfo& shaderInfo, const ShaderCompilerValidationInfo& validationInfo = ShaderCompilerValidationInfo()) noexcept;

    private:
        /// A Slang global session uses the interface slang::IGlobalSession and it represents a connection from an application 
        /// to a particular implementation of the Slang API.
        void CreateSlangGlobalSession() noexcept;

        /// Validates shader compiler output. Internally SPIRV-Tools are used to validate whether output SPIR-V words are valid
        bool ValidateCompilerSpirvOutput(const ShaderCompilerOutput& output, ETargetProfile profile, ETargetEnvironment env) const noexcept;

        Slang::ComPtr<slang::IGlobalSession> m_globalSession;
    };

} // Warp::nri namespace