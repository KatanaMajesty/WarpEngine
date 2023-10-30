#include "Shader.h"

#include "../Core/Assert.h"

namespace Warp
{

	void* CShader::GetBinaryPointer() const
	{
		WARP_ASSERT(m_binary);
		return m_binary->GetBufferPointer();
	}

	size_t CShader::GetBinarySize() const
	{
		WARP_ASSERT(m_binary);
		return m_binary->GetBufferSize();
	}

	void* CShader::GetPdbPointer() const
	{
		return m_pdb ? m_pdb->GetBufferPointer() : nullptr;
	}

	size_t CShader::GetPdbSize() const
	{
		return m_pdb ? m_pdb->GetBufferSize() : 0;
	}

	void* CShader::GetReflectionPointer() const
	{
		return m_reflection ? m_reflection->GetBufferPointer() : nullptr;
	}

	size_t CShader::GetReflectionSize() const
	{
		return m_reflection ? m_reflection->GetBufferSize() : 0;

	}

}