#include "nri_shader_compiler.h"

#include "nri_slang_util.h"

#include "common/assert.h"
#include "common/attr_defs.h"

#include "spirv-tools/libspirv.hpp"

#include <ranges>
#include <span>

namespace Warp::nri
{

    bool ShaderCompiler::Init() noexcept
    {
        this->CreateSlangGlobalSession();
        return true;
    }

    ShaderCompilerOutput ShaderCompiler::CompileSlang(const ShaderCompilerSlangInfo& shaderInfo,
                                                      const ShaderCompilerValidationInfo& validationInfo) noexcept
    {
        // For further reference on SPIR-V compilation see https://docs.shader-slang.org/en/latest/compilation-api.html
        WARP_ASSERT(m_globalSession != nullptr, "Global session was not properly initialized");

        // Firstly session creation:
        // Creating a session sets the configuration for what you are going to do with the API.
        // see ref: https://docs.shader-slang.org/en/latest/compilation-api.html#create-session
        auto ConvertTargetProfile = [this](ETargetProfile profile) -> SlangProfileID
        {
            switch (profile)
            {
            case ETargetProfile::Spv_1_5: return m_globalSession->findProfile("spirv_1_5");
            case ETargetProfile::Spv_1_6: return m_globalSession->findProfile("spirv_1_6");
            case ETargetProfile::Unknown: WARP_A_FALLTHROUGH;
            default: WARP_ASSERT(false, "Failed to find proper target profile?"); return SLANG_PROFILE_UNKNOWN;
            }
        };

        slang::TargetDesc targetDesc;
        targetDesc.format = SlangCompileTarget::SLANG_SPIRV;
        targetDesc.profile = ConvertTargetProfile(shaderInfo.targetProfile);

        slang::SessionDesc sessionDesc;
        sessionDesc.targetCount = 1;
        sessionDesc.targets = &targetDesc;
        // just indicate explicitly that we are targeting row-major
        sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_ROW_MAJOR;
        // search paths for imports and #include directives
        // we cannot just use path.c_str as they might return wchar_t on some platforms, so perform full convertion here
        static auto ConvertIncludePathToStr = [](std::filesystem::path path) { return path.string(); };
        std::vector<std::string> includePathsStrs =
            shaderInfo.includeDirectories | std::views::transform(ConvertIncludePathToStr) | std::ranges::to<std::vector>();

        static auto ConvertIncludeStrToCstr = [](const std::string& strPath) { return strPath.c_str(); };
        std::vector<const char*> includePathsCstrs =
            includePathsStrs | std::views::transform(ConvertIncludeStrToCstr) | std::ranges::to<std::vector>();

        sessionDesc.searchPaths = includePathsCstrs.data();
        sessionDesc.searchPathCount = static_cast<SlangInt>(includePathsCstrs.size());
        // preprocessor macros
        static auto ConvertMacro = [](const ShaderPreprocessorMacro& shaderMacro)
        { return slang::PreprocessorMacroDesc{ .name = shaderMacro.name.data(), .value = shaderMacro.value.data() }; };
        std::vector<slang::PreprocessorMacroDesc> macros =
            shaderInfo.preprocessorMacros | std::views::transform(ConvertMacro) | std::ranges::to<std::vector>();
        sessionDesc.preprocessorMacros = macros.data();
        sessionDesc.preprocessorMacroCount = static_cast<SlangInt>(macros.size());

        Slang::ComPtr<slang::ISession> session;
        NRI_SLANG_CHECK_RESULT(m_globalSession->createSession(sessionDesc, session.writeRef()), "Failed to create Slang session");

        // Module processing step:
        // Modules are the granularity of shader source code that can be compiled in Slang.
        // When using the compilation API, there are two main functions to consider.
        // see ref: https://docs.shader-slang.org/en/latest/compilation-api.html#load-modules
        Slang::ComPtr<slang::IModule> slangModule;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;

            // path is used as a backup key for caching the module in the session,
            // which is only likely to be used if a shader uses path-based import statements,
            // (e.g. import "../mymodule.slang";)
            //
            // Specifying nullptr as path effectively causes Slang to cache based only on moduleName.
            const char* modulePath = nullptr;
            const char* moduleName = shaderInfo.moduleName.data();
            const char* moduleCode = shaderInfo.moduleCode.data();
            slangModule = session->loadModuleFromSourceString(moduleName, modulePath, moduleCode, diagnosticsBlob.writeRef());
            WARP_ASSERT(slangModule != nullptr, "Failed to load Slang module: {}", (const char*)diagnosticsBlob->getBufferPointer());
        }

        // Modules are owned by the slang Session. Once loaded, they are valid as long as the Session is valid.

        // Now we query for entry points:
        // Slang shaders may contain many entry points, and its necessary to be able to identify them programatically
        // in the Compilation API in order to select which entry points to compile.
        // see ref: https://docs.shader-slang.org/en/latest/compilation-api.html#query-entry-points
        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        {
            const char* entryPointName = shaderInfo.entryPoint.data();
            NRI_SLANG_CHECK_RESULT(slangModule->findEntryPointByName(entryPointName, entryPoint.writeRef()),
                                   "Failed to obtain Slang entry-point {}",
                                   entryPointName);
        }

        // Compose Modules and Entry Points:
        // to move forward with defining a GPU program, the relevant subset need to be selected for composition into a unified program.
        // see ref https://docs.shader-slang.org/en/latest/compilation-api.html#compose-modules-and-entry-points
        std::array componentTypes = { static_cast<slang::IComponentType*>(slangModule.get()),
                                      static_cast<slang::IComponentType*>(entryPoint.get()) };
        Slang::ComPtr<slang::IComponentType> composedProgram;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            NRI_SLANG_CHECK_RESULT(session->createCompositeComponentType(componentTypes.data(),
                                                                         static_cast<SlangInt>(componentTypes.size()),
                                                                         composedProgram.writeRef(),
                                                                         diagnosticsBlob.writeRef()),
                                   "Failed to create composition of module & entry-point: {}",
                                   diagnosticsBlob ? (const char*)diagnosticsBlob->getBufferPointer() : "");
        }

        // Program linking:
        // see ref: https://docs.shader-slang.org/en/latest/compilation-api.html#link
        Slang::ComPtr<slang::IComponentType> linkedProgram;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            NRI_SLANG_CHECK_RESULT(composedProgram->link(linkedProgram.writeRef(), diagnosticsBlob.writeRef()),
                                   "Failed to link Slang program: {}",
                                   diagnosticsBlob ? (const char*)diagnosticsBlob->getBufferPointer() : "");
        }

        // Final step - compiling Slang module & entry-point composition into a target kernel
        // see ref: https://docs.shader-slang.org/en/latest/compilation-api.html#get-target-kernel-code
        Slang::ComPtr<slang::IBlob> spirvBlob;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            NRI_SLANG_CHECK_RESULT(linkedProgram->getEntryPointCode(0,
                                                                    /*entryPointIndex*/ 0, /*targetIndex*/
                                                                    spirvBlob.writeRef(),
                                                                    diagnosticsBlob.writeRef()),
                                   "Failed to get entry point SPIR-V binary: {}",
                                   diagnosticsBlob ? (const char*)diagnosticsBlob->getBufferPointer() : "");
        }

        std::span<const std::byte> spirvBytes = std::span((const std::byte*)spirvBlob->getBufferPointer(), spirvBlob->getBufferSize());
        WARP_ASSERT(spirvBytes.size() % 4 == 0, "SPIR-V binary MUST be 4-byte aligned (Word-aligned)");

        size_t numSpirvWords = spirvBytes.size() / 4;
        std::vector<uint32_t> spirvWords(numSpirvWords);
        std::memcpy(spirvWords.data(), spirvBytes.data(), spirvBytes.size());

        ShaderCompilerOutput output{ .spirvWords = std::move(spirvWords) };
        if (validationInfo.enableValidation)
        {
            bool validSpirv = ValidateCompilerSpirvOutput(output, shaderInfo.targetProfile, validationInfo.validationEnvironment);
            if (!validSpirv)
            {
                WARP_LOG_ERROR(ELoggerType::NriLogger, "SPIR-V validation failed for Slang module {}", shaderInfo.moduleName);
                return ShaderCompilerOutput();
            }
        }
        return output;
    }

    void ShaderCompiler::CreateSlangGlobalSession() noexcept
    {
        SlangGlobalSessionDesc globalSessionDesc = SlangGlobalSessionDesc();
        NRI_SLANG_CHECK_RESULT(slang::createGlobalSession(&globalSessionDesc, m_globalSession.writeRef()),
                               "Failed to create Slang global session");
    }

    bool ShaderCompiler::ValidateCompilerSpirvOutput(const ShaderCompilerOutput& output,
                                                     ETargetProfile profile,
                                                     ETargetEnvironment env) const noexcept
    {
        static auto SpvMessageConsumer =
            [](spv_message_level_t level, const char* source, const spv_position_t& position, const char* message)
        {
            std::string formattedMessage = std::format(
                "SPIR-V validator: {} @ line {}, col {}, i{}: {}", source, position.line, position.column, position.index, message);
            switch (level)
            {
            case SPV_MSG_FATAL: WARP_A_FALLTHROUGH;
            case SPV_MSG_INTERNAL_ERROR: WARP_A_FALLTHROUGH;
            case SPV_MSG_ERROR: WARP_LOG_ERROR(ELoggerType::NriLogger, formattedMessage); break;
            case SPV_MSG_WARNING: WARP_LOG_WARN(ELoggerType::NriLogger, formattedMessage); break;
            case SPV_MSG_INFO: WARP_A_FALLTHROUGH;
            case SPV_MSG_DEBUG: WARP_A_FALLTHROUGH;
            default: WARP_LOG_INFO(ELoggerType::NriLogger, formattedMessage);
            };
        };

        spv_target_env targetEnv = SPV_ENV_MAX;
        switch (env)
        {
        case ETargetEnvironment::Universal:
        {
            // in case with universal we also need to choose based of ETargetProfile
            switch (profile)
            {
            case ETargetProfile::Spv_1_5: targetEnv = SPV_ENV_UNIVERSAL_1_5; break;
            case ETargetProfile::Spv_1_6: targetEnv = SPV_ENV_UNIVERSAL_1_6; break;
            case ETargetProfile::Unknown: WARP_A_FALLTHROUGH;
            default: targetEnv = SPV_ENV_UNIVERSAL_1_6;
            };
            break;
        }
        case ETargetEnvironment::Vk_1_0: targetEnv = SPV_ENV_VULKAN_1_0; break;
        case ETargetEnvironment::Vk_1_1: targetEnv = SPV_ENV_VULKAN_1_1; break;
        case ETargetEnvironment::Vk_1_2: targetEnv = SPV_ENV_VULKAN_1_2; break;
        case ETargetEnvironment::Vk_1_3: targetEnv = SPV_ENV_VULKAN_1_3; break;
        case ETargetEnvironment::Vk_1_4: targetEnv = SPV_ENV_VULKAN_1_4; break;
        default: WARP_ASSERT(false, "Unknown target environment");
        }
        spvtools::SpirvTools spvTools(targetEnv);
        spvTools.SetMessageConsumer(SpvMessageConsumer);
        return spvTools.Validate(output.spirvWords);
    }

} // Warp::nri namespace