struct ViewData
{
    matrix View;
    matrix ViewInv;
    matrix Projection;
};

// see Renderer.h:EHlslDrawPropertyFlag
// TODO: Rewrite this to be cross-language?
#define DRAWFLAG_HAS_TEXCOORDS 1
#define DRAWFLAG_HAS_TANGENTS 2
#define DRAWFLAG_HAS_BITANGENTS 4
#define DRAWFLAG_NO_NORMALMAP 8
#define DRAWFLAG_NO_ROUGHNESSMETALNESSMAP 16
#define DRAWFLAG_NO_BASECOLORMAP 32

struct DrawData
{
    matrix InstanceToWorld;
    matrix NormalMatrix; // TODO: Maybe move it somewhere?
    uint DrawFlags;
};

ConstantBuffer<ViewData> CbViewData : register(b0);
ConstantBuffer<DrawData> CbDrawData : register(b1);

struct OutVertex
{
    float4 Pos : SV_Position;
    float3 PosWorld : POSITION;
    float3 Normal : NORMAL;
    float2 TexUv : TEXCOORD;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
};

struct Meshlet
{
    uint VertexCount;
    uint VertexOffset;
    uint PrimitiveCount;
    uint PrimitiveOffset;
};

StructuredBuffer<float3>    Positions   : register(t0, space0);
StructuredBuffer<float3>    Normals     : register(t0, space1);
StructuredBuffer<float2>    TexCoords   : register(t0, space2);
StructuredBuffer<float3>    Tangents    : register(t0, space3);
StructuredBuffer<float3>    Bitangents  : register(t0, space4);
StructuredBuffer<Meshlet>   Meshlets    : register(t1);
ByteAddressBuffer           UniqueVertexIndices : register(t2);
StructuredBuffer<uint>      PrimitiveIndices : register(t3);

uint3 UnpackPrimitive(uint primitive)
{
    // Unpacks a 10 bits per index triangle from a 32-bit uint.
    return uint3(primitive & 0x3FF, (primitive >> 10) & 0x3FF, (primitive >> 20) & 0x3FF);
}

uint3 GetPrimitive(Meshlet m, uint groupThreadID)
{
    return UnpackPrimitive(PrimitiveIndices[m.PrimitiveOffset + groupThreadID]);
}

uint GetVertexIndex(Meshlet m, uint localIndex)
{
    localIndex = m.VertexOffset + localIndex;
    return UniqueVertexIndices.Load(localIndex * 4);
}

OutVertex GetVertex(uint meshletIndex, uint vertexIndex)
{   
    matrix mvp = mul(CbDrawData.InstanceToWorld, mul(CbViewData.View, CbViewData.Projection));
    float4 pos = mul(float4(Positions[vertexIndex], 1.0), mvp);
    float4 posWorld = mul(float4(Positions[vertexIndex], 1.0), CbDrawData.InstanceToWorld);
    
    OutVertex v;
    v.Pos = pos;
    v.PosWorld = posWorld.xyz;
    v.Normal = normalize(mul(Normals[vertexIndex], (float3x3)CbDrawData.NormalMatrix));
    if (CbDrawData.DrawFlags & DRAWFLAG_HAS_TEXCOORDS)
        v.TexUv = TexCoords[vertexIndex];
    
    if (CbDrawData.DrawFlags & DRAWFLAG_HAS_TANGENTS)
        v.Tangent = Tangents[vertexIndex];
    
    if (CbDrawData.DrawFlags & DRAWFLAG_HAS_BITANGENTS)
        v.Bitangent = Bitangents[vertexIndex];
    
    return v;
}

[outputtopology("triangle")]
[numthreads(128, 1, 1)]
void MSMain(
    in uint groupID : SV_GroupID,
    in uint groupThreadID : SV_GroupThreadID,
	out vertices OutVertex outVerts[128],
	out indices uint3 outIndices[128])
{
    // In fact, we need to do Meshlets[offsetOfMeshlet + groupThreadID] but for cube it is good enough
    Meshlet m = Meshlets[groupID];
    
    SetMeshOutputCounts(m.VertexCount, m.PrimitiveCount);

    if (groupThreadID < m.VertexCount)
    {
        uint vertexIndex = GetVertexIndex(m, groupThreadID);
        outVerts[groupThreadID] = GetVertex(groupID, vertexIndex);
    }
    
    if (groupThreadID < m.PrimitiveCount)
    {
        outIndices[groupThreadID] = GetPrimitive(m, groupThreadID);
    }
}

Texture2D BaseColor : register(t4, space0);
Texture2D NormalMap : register(t4, space1);
Texture2D MetalnessRoughnessMap : register(t4, space2);
SamplerState StaticSampler : register(s0);

struct OutFragment
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
    float2 MetalnessRoughness : SV_Target2;
};

OutFragment PSMain(OutVertex vertex)
{
    float3 albedo = 0.0;
    if ((CbDrawData.DrawFlags & DRAWFLAG_NO_BASECOLORMAP) == 0)
    {
        albedo = BaseColor.Sample(StaticSampler, vertex.TexUv).rgb;
    }

    float3 N = normalize(vertex.Normal);
    if ((CbDrawData.DrawFlags & DRAWFLAG_NO_NORMALMAP) == 0 && 
        (CbDrawData.DrawFlags & DRAWFLAG_HAS_TANGENTS) && 
        (CbDrawData.DrawFlags & DRAWFLAG_HAS_BITANGENTS))
    {
        float3 tangent = normalize(vertex.Tangent);
        float3 bitangent = normalize(vertex.Bitangent);
        float3x3 TBN = float3x3(tangent, bitangent, N);
        float3 NSample = NormalMap.Sample(StaticSampler, vertex.TexUv).xyz * 2.0 - 1.0;
        N = normalize(mul(NSample, TBN));
    }
    
    float2 metalnessRoughness = float2(0.0, 1.0);
    if ((CbDrawData.DrawFlags & DRAWFLAG_NO_ROUGHNESSMETALNESSMAP) == 0)
    {
        // TODO: Afaik currently we have xy - metalness, zw - roughness. Would be nice to change this to float2 from beginning (Gltf issues)
        metalnessRoughness = MetalnessRoughnessMap.Sample(StaticSampler, vertex.TexUv).xz;
    }
    
    OutFragment output;
    output.Albedo = float4(albedo, 0.0);
    output.Normal = float4(N, 0.0);
    output.MetalnessRoughness = metalnessRoughness;
    return output;
}