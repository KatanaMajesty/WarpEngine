#pragma once

#include "RHI/stdafx.h"
#include <dxcapi.h>

namespace Warp
{

    // https://developer.nvidia.com/dx12-dos-and-donts

    enum class EShaderModel
    {
        sm_6_5, // nothing else is needed
    };

    // We do not use Hull, Domain, Geometry shaders here (at least for now)
    enum class EShaderType
    {
        Unknown = 0,
        Vertex,
        Amplification,
        Mesh,
        Pixel,
        Compute,
    };

    // Shader is just bytecode + bytecode size. we use a tool called ShaderCompiler to get a Shader object
    class CShader
    {
    public:
        CShader() = default;
        CShader(EShaderType type, ComPtr<IDxcBlob> binary, ComPtr<IDxcBlob> pdb, ComPtr<IDxcBlob> reflection)
            : m_shaderType(type)
            , m_binary(binary)
            , m_pdb(pdb)
            , m_reflection(reflection)
        {
        }

        inline constexpr EShaderType GetType() const { return m_shaderType; }
        inline D3D12_SHADER_BYTECODE GetBinaryBytecode() { return CD3DX12_SHADER_BYTECODE(GetBinaryPointer(), GetBinarySize()); }

        inline bool HasBinary() const { return m_binary != nullptr; }

        void* GetBinaryPointer() const;
        size_t GetBinarySize() const;

    private:
        EShaderType m_shaderType = EShaderType::Unknown;
        ComPtr<IDxcBlob> m_binary;
        ComPtr<IDxcBlob> m_pdb;
        ComPtr<IDxcBlob> m_reflection;
    };

}