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
    float3 N = GbufferNormal.Sample(StaticSampler, vertex.Uv).xyz;
    float3 V = normalize(eye - worldPos);
    // TODO: Add roughness/metalness when switching to Pbr
    
    float3 color = 0.0;
    for (uint i = 0; i < CbLightEnv.NumDirLights; ++i)
    {
        DirectionalLight light = CbLightEnv.DirLights[i];
  
        // Shadows
        matrix lightMatrix = mul(light.LightView, light.LightProj);
        float4 lightpos = mul(float4(worldPos, 1.0), lightMatrix);
        float3 projCoords = lightpos.xyz / lightpos.w;
  
        float2 uv = float2(projCoords.x * 0.5 + 0.5, projCoords.y * -0.5 + 0.5);
        float3 shadow = DirectionalShadowmaps[i].SampleCmpLevelZero(ShadowmapSampler, uv, projCoords.z);
  
        // Lighting
        float3 L = normalize(-light.Direction);
        float3 H = normalize(L + V);

        float lambertian = saturate(dot(N, L));
        float3 diffuse = light.Intensity * light.Radiance * lambertian * albedo;

        float specAngle = saturate(dot(H, N));
        float specular = pow(specAngle, 32.0);

        const float3 ambientColor = float3(0.03, 0.03, 0.03);
        color += shadow * (ambientColor + diffuse + specular);
    }
    
    return float4(color, 1.0); // TODO: Add support of alpha from albedo gbuffer (??)
}