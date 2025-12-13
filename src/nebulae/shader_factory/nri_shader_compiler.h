#pragma once

#include <slang/slang.h>
#include <slang/slang-com-ptr.h>

#include <span>
#include <string_view>
#include <vector>

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

    enum class ESpvProfile
    {
        Unknown,
        Spirv_1_5,
        Spirv_1_6,
    };

    struct SlangToSpvValidationInfo
    {
        /// empty struct, currently just informs compiler to either perform validation or not
    };

    struct SlangToSpvInfo
    {
        /// @brief A source code of Slang shader (read by the client)
        std::string_view moduleCode;
        std::string_view moduleName;
        std::string_view entryPointName;

        ESpvProfile targetProfile = ESpvProfile::Unknown;
        
        std::span<const std::string_view> includeDirectories;
        std::span<const ShaderPreprocessorMacro> preprocessorMacros;

        /// @brief Optional pointer to validation information.
        /// If nullptr is provided no SPIRV validation will be performed
        const SlangToSpvValidationInfo* validationInfo = nullptr;
    };

    struct SlangToSpvOutput
    {
        std::vector<uint32_t> spirvWords;
    };

    class ShaderCompiler
    {
    private:
        ShaderCompiler() = default;

    public:
        ShaderCompiler(const ShaderCompiler&) = delete;
        ShaderCompiler& operator=(const ShaderCompiler&) = delete;

        static ShaderCompiler* Get() noexcept
        {
            static ShaderCompiler instance;
            return &instance;
        }

        /// @brief Initializes Slang context and global session.
        bool Init() noexcept;

        /// @brief Compiles Slang to SPIR-V module
        SlangToSpvOutput CompileSlangToSpv(const SlangToSpvInfo& compileInfo) noexcept;

    private:
        /// A Slang global session uses the interface slang::IGlobalSession and it represents a connection from an application 
        /// to a particular implementation of the Slang API.
        void CreateSlangGlobalSession() noexcept;

        bool ValidateSlangToSpv(const SlangToSpvValidationInfo* validationInfo, const SlangToSpvOutput& slangToSpvOutput) const noexcept;

        Slang::ComPtr<slang::IGlobalSession> m_globalSession;
    };

} // Warp::nri namespace