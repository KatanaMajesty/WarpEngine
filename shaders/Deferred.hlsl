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

float3 WorldPosFromDepth(float2 uv, float depth)
{
    float4 ndc = float4(uv * 2.0 - 1.0, depth, 1.0);
    ndc.y = -ndc.y; // Inverse .y as we are in HLSL
    
    // TODO: Maybe we want to precalculate viewProj matrix? Not to good to do this here
    matrix viewProjInv = mul(CbViewData.ProjInv, CbViewData.ViewInv);
    float4 world = mul(ndc, viewProjInv);
    float3 worldPos = world.xyz / world.w;
    return worldPos;
}

float4 PSMain(OutVertex vertex) : SV_Target0
{
    float sceneDepth = SceneDepth.SampleLevel(StaticSampler, vertex.Uv, 0.0);
    float3 worldPos = WorldPosFromDepth(vertex.Uv, sceneDepth);
    float3 eye = CbViewData.ViewInv[3].xyz;
    
    float3 albedo = GbufferAlbedo.Sample(StaticSampler, vertex.Uv).rgb;
    float2 roughnessMetalness = GbufferRoughnessMetalness.Sample(StaticSampler, vertex.Uv);
    float3 N = normalize(GbufferNormal.Sample(StaticSampler, vertex.Uv).xyz);
    float3 V = normalize(eye - worldPos);
    float VdotN = saturate(dot(V, N));
    
    const float3 ambient = 0.04;
    float3 Lo = 0.0;
    for (uint i = 0; i < CbLightEnv.NumDirLights; ++i)
    {
        DirectionalLight light = CbLightEnv.DirLights[i];
  
        // Lighting
        float3 L = normalize(-light.Direction);
        float3 H = normalize(L + V);
        float LdotN = saturate(dot(L, N));
        float VdotH = saturate(dot(V, H));
        float NdotH = saturate(dot(N, H));
        float NdotL = saturate(dot(N, L));
        
        // To handle both metals and non metals, we consider a purely non metallic surface to have base reflectivity of (0.04, 0.04, 0.04) 
        // and lerp between this value of base reflectivity (F0) based on the metallic factor of the surface.
        float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, roughnessMetalness.y);
        float3 F = Brdf_FresnelSchlick(F0, VdotH);
        float3 Kd = 1.0 - F; // We assume F to represent Ks -> Kd = 1.0 - Ks
        float3 O = Kd * Brdf_Diffuse_Lambertian(albedo) + Brdf_Specular_CookTorrance(F, roughnessMetalness.x, VdotN, LdotN, VdotH, NdotH);
        
        // Shadows
        matrix lightMatrix = mul(light.LightView, light.LightProj);
        float4 lightpos = mul(float4(worldPos, 1.0), lightMatrix);
        float3 projCoords = lightpos.xyz / lightpos.w;
  
        float2 uv = float2(projCoords.x * 0.5 + 0.5, projCoords.y * -0.5 + 0.5);
        float3 shadow = DirectionalShadowmaps[i].SampleCmpLevelZero(ShadowmapSampler, uv, projCoords.z);
  
        Lo += shadow * O * NdotL * light.Radiance * light.Intensity;
    }
    
    return float4(ambient + Lo, 1.0); // TODO: Add support of alpha from albedo gbuffer (??)
}