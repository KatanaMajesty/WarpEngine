#include "nri_shader_library.h"

namespace Warp::nri
{

    bool ShaderLibrary::Init() noexcept
    {
        m_shaderPath = std::filesystem::path("shaders");
        return true;
    }

} // Warp::nri namespace