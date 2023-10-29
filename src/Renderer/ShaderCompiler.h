#pragma once

#include <vector>
#include <string_view>

// stdafx should be included before dxcapi to handle API correctly
#include "RHI/stdafx.h"
#include <dxcapi.h>

#include "Shader.h"

namespace Warp
{

	// For better understanding of DXC https://simoncoenen.com/blog/programming/graphics/DxcCompiling
	
	enum EShaderCompilationFlags
	{
		eShaderCompilationFlags_None = 0,

		// Strips PDBs from the Object
		eShaderCompilationFlags_StripDebug = 1,

		// Strips reflection data from Object
		eShaderCompilationFlags_StripReflect = 2,

		// Only 1 optimization flag should be specified, otherwise the highest optimization level will be selected
		eShaderCompilationFlags_O0 = 4,
		eShaderCompilationFlags_O1 = 8,
		eShaderCompilationFlags_O2 = 16,
		eShaderCompilationFlags_O3 = 32,

#ifdef WARP_DEBUG
		eShaderCompilationFlags_Suitable = eShaderCompilationFlags_O0,
#else
		eShaderCompilationFlags_Suitable = eShaderCompilationFlags_O3 | eShaderCompilationFlags_StripDebug | eShaderCompilationFlags_StripReflect,
#endif
	};

	// Shader define should only live in a scope of shader compilation
	// TODO: Maybe somehow replace convertion from std::string_view to std::wstring?
	struct ShaderDefine
	{
		ShaderDefine() = default;
		ShaderDefine(std::string_view name, std::string_view value)
			: Name(name.begin(), name.end())
			, Value(value.begin(), value.end())
		{
		}

		std::wstring Name;
		std::wstring Value;
	};

	struct ShaderCompilationDesc
	{
		ShaderCompilationDesc() = default;
		ShaderCompilationDesc(std::string_view entryPoint, EShaderModel shaderModel, EShaderType shaderType)
			: EntryPoint(entryPoint)
			, ShaderModel(ShaderModel)
			, ShaderType(shaderType)
		{
		}

		inline ShaderCompilationDesc& AddDefine(const ShaderDefine& define)
		{
			Defines.push_back(define);
			return *this;
		}

		std::vector<ShaderDefine> Defines;
		const std::string_view EntryPoint;
		const EShaderModel ShaderModel;
		const EShaderType ShaderType;
	};

	class CShaderCompiler
	{
	public:
		CShaderCompiler();

		CShaderCompiler(const CShaderCompiler&) = delete;
		CShaderCompiler& operator=(const CShaderCompiler&) = delete;

		~CShaderCompiler() = default;

		CShader CompileShader(const std::string& filepath, 
			const ShaderCompilationDesc& desc, 
			EShaderCompilationFlags flags = eShaderCompilationFlags_Suitable);
		
	private:
		std::wstring_view GetTargetProfile(EShaderModel shaderModel, EShaderType shaderType) const;

		struct CompilationResult
		{
			ComPtr<IDxcBlob> Binary;
			ComPtr<IDxcBlob> Pdb;
			ComPtr<IDxcBlob> Reflection;
			//IDxcBlob - RDAT part with reflection data
		};

		CompilationResult CompileInternal(
			std::wstring_view filepath,
			std::wstring_view entryPoint,
			std::wstring_view targetProfile,
			const std::vector<DxcDefine>& defines, EShaderCompilationFlags flags);

		ComPtr<struct IDxcUtils> m_DxcUtils;
		ComPtr<struct IDxcCompiler3> m_DxcCompiler;
		ComPtr<struct IDxcIncludeHandler> m_DxcIncludeHandler;
	};

}