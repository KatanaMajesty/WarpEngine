#pragma once

#include "RHI/RootSignature.h"
#include "RHI/PipelineState.h"

namespace Warp
{

	class Material
	{
	public:
		Material() = default;
	
	private:
		RHIRootSignature m_rootSignature;
		RHIMeshPipelineState m_PSO;
	};

}