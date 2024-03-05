struct OutVertex
{
    float4 Pos : SV_Position;
    float2 Uv : TEXCOORD;
};

// .xy - pos, .zw - uv
static const float4 FullscreenTriangle[3] =
{
    float4(-1.0,  1.0, 0.0, 0.0),
    float4( 3.0,  1.0, 2.0, 0.0),
    float4(-1.0, -3.0, 0.0, 2.0),
};

OutVertex VSMain(uint vid : SV_VertexID)
{
    OutVertex output;
    output.Pos = float4(FullscreenTriangle[vid].xy, 0.0, 1.0);
    output.Uv = FullscreenTriangle[vid].zw;
    return output;
}

struct ViewData
{
    matrix ViewInv;
    matrix ProjInv;
};

struct DirectionalLight
{
    matrix LightView;
    matrix LightProj;
    float Intensity;
    float3 Direction;
    float3 Radiance;
};

struct LightEnv
{
    uint NumDirLights;
    DirectionalLight DirLights[3];
};

ConstantBuffer<ViewData> CbViewData : register(b0);
ConstantBuffer<LightEnv> CbLightEnv : register(b1);

Texture2D<float4> GbufferAlbedo : register(t0, space0);
Texture2D<float4> GbufferNormal : register(t0, space1);
Texture2D<float2> GbufferRoughnessMetalness : register(t0, space2);
Texture2D<float>  SceneDepth : register(t1);
SamplerState StaticSampler : register(s0);

// TODO: Instead we want screen-space shadows
Texture2D<float> DirectionalShadowmaps[3] : register(t2, space0);
SamplerComparisonState ShadowmapSampler : register(s1);

#include "BRDF.hlsli"
#include "OctahedronEncoding.hlsli"

float3 WorldPosFromDepth(in matrix viewProjInv, in float2 uv, in float depth)
{
    float4 ndc = float4(uv * 2.0 - 1.0, depth, 1.0);
    ndc.y = -ndc.y; // Inverse .y as we are in HLSL
    
    float4 world = mul(ndc, viewProjInv);
    float3 worldPos = world.xyz / world.w;
    return worldPos;
}

float4 PSMain(OutVertex vertex) : SV_Target0
{
    // Frustum calculation for texel size
    matrix viewProjInv = mul(CbViewData.ProjInv, CbViewData.ViewInv);
    
    float sceneDepth = SceneDepth.SampleLevel(StaticSampler, vertex.Uv, 0.0);
    float3 worldPos = WorldPosFromDepth(viewProjInv, vertex.Uv, sceneDepth);
    float3 eye = CbViewData.ViewInv[3].xyz;
    
    float3 albedo = GbufferAlbedo.Sample(StaticSampler, vertex.Uv).rgb;
    float2 roughnessMetalness = GbufferRoughnessMetalness.Sample(StaticSampler, vertex.Uv);
    
    // As of 16.02.2024 -> .xy - geometry normal packed, .zw - surface normal packed
    float4 normals = GbufferNormal.Sample(StaticSampler, vertex.Uv);
    float3 GN = Oct16_FastUnpack(normals.xy);
    float3 SN = Oct16_FastUnpack(normals.zw);
    float3 V = normalize(eye - worldPos);
    float VdotN = clamp(dot(V, SN), 0.00001, 1.0);
    
    float3 Lo = 0.0;
    for (uint i = 0; i < CbLightEnv.NumDirLights; ++i)
    {
        DirectionalLight light = CbLightEnv.DirLights[i];
  
        // Lighting
        float3 L = normalize(-light.Direction);
        float3 H = normalize(L + V);
        float LdotN = clamp(dot(L, SN), 0.00001, 1.0);
        float VdotH = clamp(dot(V, H), 0.00001, 1.0);
        float NdotH = clamp(dot(SN, H), 0.00001, 1.0);
        
        // To handle both metals and non metals, we consider a purely non metallic surface to have base reflectivity of (0.04, 0.04, 0.04) 
        // and lerp between this value of base reflectivity (F0) based on the metallic factor of the surface.
        float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, roughnessMetalness.g);
        float3 F = Brdf_FresnelSchlick(F0, VdotH);
        float3 Kd = 1.0 - F; // We assume F to represent Ks -> Kd = 1.0 - Ks
        float3 O = Kd * Brdf_Diffuse_Lambertian(albedo) + Brdf_Specular_CookTorrance(F, roughnessMetalness.r, VdotN, LdotN, VdotH, NdotH);
        
        // Shadows
        float2 shadowMapResolution;
        DirectionalShadowmaps[i].GetDimensions(shadowMapResolution.x, shadowMapResolution.y);
        
        float2 frustumScaling = float2(light.LightProj[0][0], light.LightProj[1][1]);
        float2 shadowMapTexelSize = 2.0 / (frustumScaling * shadowMapResolution);
        
        // https://user-images.githubusercontent.com/7088062/36948961-df37690e-1fea-11e8-8999-af8af60403fb.png
        // Normal offsetting to help us breathe
        float LdotGN = clamp(dot(L, GN), 0.0001, 1.0);
        float3 NOffsetScale = max(shadowMapTexelSize.x, shadowMapTexelSize.y) * sqrt(2.0) / 2.0 * (GN + 0.9 * L * LdotGN);
        float3 NOffset = GN * NOffsetScale;
        
        // Offset in uv space only
        matrix lightMatrix = mul(light.LightView, light.LightProj);
        float4 lightpos = mul(float4(worldPos + NOffset, 1.0), lightMatrix);
        
        float3 projCoords = lightpos.xyz / lightpos.w;
        float2 uv = float2(projCoords.x * 0.5 + 0.5, projCoords.y * -0.5 + 0.5);
        
        float depth = projCoords.z;
        float shadow = DirectionalShadowmaps[i].SampleCmpLevelZero(ShadowmapSampler, uv, depth);
        Lo += shadow * O * LdotN * light.Radiance * light.Intensity;
    }
    
    const float3 ambient = float3(0.12, 0.1, 0.06) * albedo;
    return float4(ambient + Lo, 1.0); // TODO: Add support of alpha from albedo gbuffer (??)
}