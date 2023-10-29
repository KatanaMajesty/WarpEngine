#pragma once

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

	private:
		// Blob, ptr, whatever
	};

}