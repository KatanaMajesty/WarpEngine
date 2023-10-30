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

}