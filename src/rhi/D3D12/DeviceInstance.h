#pragma once

namespace Warp::rhi
{

    enum class ESupportTier_MeshShader
    {
        NotSupported = 0,
        SupportTier_1_0 = 1,
    };

    enum class ESupportTier_Raytracing
    {
        NotSupported = 0,
        SupportTier_1_0 = 1,
        SupportTier_1_1 = 2
    };

    struct D3D12DeviceCapabilities
    {
        ESupportTier_MeshShader MeshShaderSupportTier = ESupportTier_MeshShader::NotSupported;
        ESupportTier_Raytracing RayTracingSupportTier = ESupportTier_Raytracing::NotSupported;
    };

    class D3D12DeviceInstance
    {
    public:
        D3D12DeviceInstance() = default;

        D3D12DeviceInstance(const D3D12DeviceInstance&) = delete;
        D3D12DeviceInstance& operator=(const D3D12DeviceInstance&) = delete;
    };

} // Warp::rhi namespace