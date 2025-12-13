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

    SlangToSpvOutput ShaderCompiler::CompileSlangToSpv(const SlangToSpvInfo& compileInfo) noexcept
    {
        // For further reference on SPIR-V compilation see https://docs.shader-slang.org/en/latest/compilation-api.html
        WARP_ASSERT(m_globalSession != nullptr, "Global session was not properly initialized");

        // Firstly session creation:
        // Creating a session sets the configuration for what you are going to do with the API.
        // see ref: https://docs.shader-slang.org/en/latest/compilation-api.html#create-session
        auto ConvertTargetProfile = [this](ESpvProfile profile) -> SlangProfileID
        {
            switch (profile)
            {
            case ESpvProfile::Spirv_1_5: return m_globalSession->findProfile("spirv_1_5");
            case ESpvProfile::Spirv_1_6: return m_globalSession->findProfile("spirv_1_6");
            case ESpvProfile::Unknown:
                WARP_A_FALLTHROUGH;
            default:
                WARP_ASSERT(false, "Failed to find proper target profile?");
                return SLANG_PROFILE_UNKNOWN;
            }
        };

        slang::TargetDesc targetDesc;
        targetDesc.format = SlangCompileTarget::SLANG_SPIRV;
        targetDesc.profile = ConvertTargetProfile(compileInfo.targetProfile);

        slang::SessionDesc sessionDesc;
        sessionDesc.targetCount = 1;
        sessionDesc.targets = &targetDesc;
        // just indicate explicitly that we are targeting row-major
        sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_ROW_MAJOR;
        // search paths for imports and #include directives
        static auto ConvertIncludePath = [](std::string_view path)
        {
            return path.data();
        };
        std::vector<const char*> includePathsStrs = compileInfo.includeDirectories |
                                                    std::views::transform(ConvertIncludePath) |
                                                    std::ranges::to<std::vector>();
        sessionDesc.searchPaths = includePathsStrs.data();
        sessionDesc.searchPathCount = static_cast<SlangInt>(includePathsStrs.size());
        // preprocessor macros
        static auto ConvertMacro = [](const ShaderPreprocessorMacro& shaderMacro)
        {
            return slang::PreprocessorMacroDesc{ .name = shaderMacro.name.data(), .value = shaderMacro.value.data() };
        };
        std::vector<slang::PreprocessorMacroDesc> macros = compileInfo.preprocessorMacros |
                                                           std::views::transform(ConvertMacro) |
                                                           std::ranges::to<std::vector>();
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
            const char* moduleName = compileInfo.moduleName.data();
            const char* moduleCode = compileInfo.moduleCode.data();
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
            const char* entryPointName = compileInfo.entryPointName.data();
            NRI_SLANG_CHECK_RESULT(slangModule->findEntryPointByName(entryPointName, entryPoint.writeRef()),
                "Failed to obtain Slang entry-point {}", entryPointName);
        }

        // Compose Modules and Entry Points:
        // to move forward with defining a GPU program, the relevant subset need to be selected for composition into a unified program.
        // see ref https://docs.shader-slang.org/en/latest/compilation-api.html#compose-modules-and-entry-points
        std::array componentTypes = {
            static_cast<slang::IComponentType*>(slangModule.get()),
            static_cast<slang::IComponentType*>(entryPoint.get())
        };
        Slang::ComPtr<slang::IComponentType> composedProgram;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            NRI_SLANG_CHECK_RESULT(session->createCompositeComponentType(
                                       componentTypes.data(),
                                       static_cast<SlangInt>(componentTypes.size()),
                                       composedProgram.writeRef(),
                                       diagnosticsBlob.writeRef()),
                "Failed to create composition of module & entry-point: {}", diagnosticsBlob ? (const char*)diagnosticsBlob->getBufferPointer() : "");
        }

        // Program linking:
        // see ref: https://docs.shader-slang.org/en/latest/compilation-api.html#link
        Slang::ComPtr<slang::IComponentType> linkedProgram;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            NRI_SLANG_CHECK_RESULT(composedProgram->link(linkedProgram.writeRef(), diagnosticsBlob.writeRef()),
                "Failed to link Slang program: {}", diagnosticsBlob ? (const char*)diagnosticsBlob->getBufferPointer() : "");
        }

        // Final step - compiling Slang module & entry-point composition into a target kernel
        // see ref: https://docs.shader-slang.org/en/latest/compilation-api.html#get-target-kernel-code
        Slang::ComPtr<slang::IBlob> spirvBlob;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            NRI_SLANG_CHECK_RESULT(linkedProgram->getEntryPointCode(0, /*entryPointIndex*/ 0, /*targetIndex*/
                                       spirvBlob.writeRef(),
                                       diagnosticsBlob.writeRef()),
                "Failed to get entry point SPIR-V binary: {}", diagnosticsBlob ? (const char*)diagnosticsBlob->getBufferPointer() : "");
        }

        std::span<const std::byte> spirvBytes = std::span((const std::byte*)spirvBlob->getBufferPointer(), spirvBlob->getBufferSize());
        WARP_ASSERT(spirvBytes.size() % 4 == 0, "SPIR-V binary MUST be 4-byte aligned (Word-aligned)");

        size_t numSpirvWords = spirvBytes.size() / 4;
        std::vector<uint32_t> spirvWords(numSpirvWords);
        std::memcpy(spirvWords.data(), spirvBytes.data(), spirvBytes.size());

        SlangToSpvOutput output;
        output.spirvWords = std::move(spirvWords);

        WARP_ASSERT(this->ValidateSlangToSpv(compileInfo.validationInfo, output), "Failed to validate SPIR-V binary");
        return output;
    }

    void ShaderCompiler::CreateSlangGlobalSession() noexcept
    {
        SlangGlobalSessionDesc globalSessionDesc = SlangGlobalSessionDesc();
        NRI_SLANG_CHECK_RESULT(slang::createGlobalSession(&globalSessionDesc, m_globalSession.writeRef()),
            "Failed to create Slang global session");
    }

    bool ShaderCompiler::ValidateSlangToSpv(const SlangToSpvValidationInfo* validationInfo, const SlangToSpvOutput& slangToSpvOutput) const noexcept
    {
        if (!validationInfo)
        {
            // dont do anything if no validation request
            return true;
        }

        static auto SpvMessageConsumer = [](spv_message_level_t level, const char* source, const spv_position_t& position, const char* message)
        {
            // TODO: Change log level to properly align with spv_message_level_t
            WARP_LOG_WARN(ELoggerType::NriLogger, "SPIR-V validator: {} @ line {}, col {}, i{}: {}",
                source,
                position.line,
                position.column,
                position.index, message);
        };

        spvtools::SpirvTools spvTools(SPV_ENV_VULKAN_1_4);
        spvTools.SetMessageConsumer(SpvMessageConsumer);
        return spvTools.Validate(slangToSpvOutput.spirvWords);
    }

} // Warp::nri namespace