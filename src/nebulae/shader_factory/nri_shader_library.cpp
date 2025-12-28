#include "nri_shader_library.h"

#include "common/assert.h"
#include "common/filesystem_io.h"

namespace Warp::nri
{
    ShaderLibrary::~ShaderLibrary()
    {
        WARP_ASSERT(!m_isInitialized, "Shader library was not properly deinitialized prior to its destructor!");
    }

    void ShaderLibrary::Init(const ShaderLibraryConfig& config) noexcept
    {
        // create shader compiler global session here
        ShaderCompilerGlobalSession::Init();

        WARP_ASSERT(!s_instance, "Shader library was already previously allocated");
        s_instance = std::unique_ptr<ShaderLibrary>(new ShaderLibrary(config));
    }

    ShaderLibrary* ShaderLibrary::Get() noexcept
    {
        WARP_ASSERT(s_instance && s_instance->m_isInitialized,
                    "Shader library was not properly initialized prior to ShaderLibrary::Get call");
        return s_instance.get();
    }

    void ShaderLibrary::Deinit() noexcept
    {
        ShaderLibrary* instance = ShaderLibrary::Get();
        instance->m_isInitialized = false;
        instance->m_config = ShaderLibraryConfig();

        // destroy shader compiler global session afterwards
        ShaderCompilerGlobalSession::Destroy();
    }

    ShaderCompilerOutput ShaderLibrary::CompileFromLibrary(EShaderLang lang,
                                                           const std::filesystem::path& shaderName,
                                                           std::string_view entryPoint,
                                                           std::span<const ShaderPreprocessorMacro> preprocessorMacros) noexcept
    {
        // first check shader lang
        switch (lang)
        {
        case EShaderLang::Slang: return CompileSlangFromLibrary(shaderName, entryPoint, preprocessorMacros);
        default: WARP_ASSERT(false, "Unknown shader language");
        }
        return ShaderCompilerOutput();
    }

    ShaderCompilerOutput ShaderLibrary::CompileSlangFromLibrary(const std::filesystem::path& shaderName,
                                                                std::string_view entryPoint,
                                                                std::span<const ShaderPreprocessorMacro> preprocessorMacros) noexcept
    {
        static constexpr std::string_view SlangFileExtension = ".slang";
        WARP_ASSERT(shaderName.has_extension() && shaderName.extension() == SlangFileExtension,
                    "Incorrect Slang shader extension of a file {}!",
                    shaderName.string());

        std::filesystem::path filepath = m_config.shaderPath / "slang_shaders" / shaderName;
        std::string shaderPath = filepath.string();
        std::string moduleCode = fsio::ReadFile(filepath);
        std::array includeDirectories = { m_config.shaderPath / "slang_shaders" };

        ShaderCompilerSlangInfo shaderInfo;
        shaderInfo.moduleCode = moduleCode;
        shaderInfo.shaderPath = shaderPath;
        // shaderInfo.moduleName = moduleName;
        shaderInfo.entryPoint = entryPoint;
        shaderInfo.targetProfile = m_config.targetProfile;
        shaderInfo.includeDirectories = includeDirectories;
        shaderInfo.preprocessorMacros = preprocessorMacros;
        // TODO: For now in Warp shader libraries we always enable validation even if WARP_ENGINE_DEBUG is not defined!
        ShaderCompilerValidationInfo validationInfo{ .enableValidation = true, .validationEnvironment = m_config.targetEnv };
        ShaderCompilerOutput compiledOutput = m_compiler.CompileSlang(shaderInfo, validationInfo);
        if (m_config.writeSpirvOutputs)
        {
            WriteSpirvOutputToLibrary(shaderName, entryPoint, compiledOutput);
        }
        return compiledOutput;
    }

    void ShaderLibrary::WriteSpirvOutputToLibrary(const std::filesystem::path& shaderName,
                                                  std::string_view entryPoint,
                                                  const ShaderCompilerOutput& output)
    {
        static constexpr std::string_view SpirvFileExtension = ".spv";
        std::string spirvName = std::format("{}_{}", shaderName.stem().string(), entryPoint);
        std::filesystem::path spirvPath = m_config.shaderPath / "spirv" / spirvName;
        // patch extension to match binary .spv ext
        // May throw implementation-defined exceptions
        spirvPath.replace_extension(SpirvFileExtension);

        fsio::WriteBinaryFile(spirvPath, output.spirvWords);
    }

} // Warp::nri namespace