#include "ShaderCompiler.h"

#pragma comment(lib, "dxcompiler.lib")

#include "../Core/Defines.h"
#include "../Core/Logger.h"
#include "../Core/Assert.h"
#include "../Util/String.h"

namespace Warp
{

	CShaderCompiler::CShaderCompiler()
	{
		WARP_RHI_VALIDATE(
			DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(m_DxcUtils.GetAddressOf()))
		);
		WARP_RHI_VALIDATE(
			DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(m_DxcCompiler.GetAddressOf()))
		);
		WARP_RHI_VALIDATE(
			m_DxcUtils->CreateDefaultIncludeHandler(m_DxcIncludeHandler.GetAddressOf())
		);
	}

	CShader CShaderCompiler::CompileShader(const std::string& filepath, const ShaderCompilationDesc& desc, EShaderCompilationFlags flags)
	{
		std::wstring wFilepath = StringToWString(filepath);
		std::wstring wEntryPoint = StringToWString(desc.EntryPoint);
		std::wstring_view wTargetProfile = GetTargetProfile(desc.ShaderModel, desc.ShaderType);

		std::vector<DxcDefine> dxcDefines;
		dxcDefines.reserve(desc.Defines.size());
		for (const auto& [name, value] : desc.Defines)
		{
			dxcDefines.emplace_back(name.data(), value.data());
		}

		CompilationResult result = CompileInternal(wFilepath, wEntryPoint, wTargetProfile, dxcDefines, flags);
		return CShader();
	}

	std::wstring_view CShaderCompiler::GetTargetProfile(EShaderModel shaderModel, EShaderType shaderType) const
	{
		switch (shaderModel)
		{
		case EShaderModel::sm_6_5:
		{
			switch (shaderType)
			{
			case EShaderType::Vertex: return L"vs_6_5";
			case EShaderType::Amplification: return L"as_6_5";
			case EShaderType::Mesh: return L"ms_6_5";
			case EShaderType::Pixel: return L"ps_6_5";
			case EShaderType::Compute: return L"cs_6_5";
			};
		}; break;
		default: WARP_ASSERT(false); return L"";
		};
	}

	CShaderCompiler::CompilationResult CShaderCompiler::CompileInternal(
		std::wstring_view filepath,
		std::wstring_view entryPoint,
		std::wstring_view targetProfile,
		const std::vector<DxcDefine>& defines, EShaderCompilationFlags flags)
	{
		std::vector<LPCWSTR> dxcArguments;
		if (flags & eShaderCompilationFlags_StripDebug)
			dxcArguments.push_back(L"-Qstrip_debug");

		if (flags & eShaderCompilationFlags_StripReflect)
			dxcArguments.push_back(L"-Qstrip_reflect");

		if (flags & eShaderCompilationFlags_O3)
		{
			dxcArguments.push_back(L"-O3");
		}
		else if (flags & eShaderCompilationFlags_O2)
		{
			dxcArguments.push_back(L"-O2");
		}
		else if (flags & eShaderCompilationFlags_O1)
		{
			dxcArguments.push_back(L"-O1");
		}
		else if (flags & eShaderCompilationFlags_O0)
		{
			dxcArguments.push_back(L"-O0");
		}

		ComPtr<IDxcCompilerArgs> compilerArgs;
		WARP_RHI_VALIDATE(m_DxcUtils->BuildArguments(
			filepath.data(),
			entryPoint.data(),
			targetProfile.data(),
			dxcArguments.data(), static_cast<UINT32>(dxcArguments.size()),
			defines.data(), static_cast<UINT32>(defines.size()),
			compilerArgs.GetAddressOf())
		);

		ComPtr<IDxcBlobEncoding> srcBlobEncoding;
		WARP_RHI_VALIDATE(m_DxcUtils->LoadFile(filepath.data(), 0, srcBlobEncoding.GetAddressOf()));

		const DxcBuffer srcBuffer = {
			.Ptr = srcBlobEncoding->GetBufferPointer(),
			.Size = srcBlobEncoding->GetBufferSize(),
			.Encoding = 0
		};

		ComPtr<IDxcResult> compilationResult;
		WARP_RHI_VALIDATE(m_DxcCompiler->Compile(&srcBuffer,
			compilerArgs->GetArguments(),
			compilerArgs->GetCount(),
			m_DxcIncludeHandler.Get(),
			IID_PPV_ARGS(compilationResult.GetAddressOf()))
		);

		CompilationResult result{};

		HRESULT status;
		WARP_RHI_VALIDATE(compilationResult->GetStatus(&status));
		if (FAILED(status))
		{
			ComPtr<IDxcBlobUtf8> errorBlob;
			WARP_RHI_VALIDATE(compilationResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errorBlob.GetAddressOf()), nullptr));
			WARP_LOG_ERROR("[CShaderCompiler] Failed to compile a shader: {}", (char*)errorBlob->GetStringPointer());
			return result; // return empty result;
		}
		
		WARP_RHI_VALIDATE(compilationResult->GetResult(result.Binary.GetAddressOf()));

		// TODO: If needed, add pdb and reflection parse
		// If reflection of debug data was stripped, process it
		/*if (flags & eShaderCompilationFlags_StripDebug)
		{

		}

		if (flags & eShaderCompilationFlags_StripReflect)
		{

		}*/
		return result;
	}

}