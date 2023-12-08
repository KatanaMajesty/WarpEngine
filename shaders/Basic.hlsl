struct ViewData
{
    matrix View;
    matrix Projection;
};

struct DrawData
{
    matrix MeshToWorld;
};

ConstantBuffer<ViewData> CbViewData : register(b0);
ConstantBuffer<DrawData> CbDrawData : register(b1);

struct OutVertex
{
    float4 pos : SV_Position;
    float3 color : COLOR0;
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

float4 TransformPosition(float3 xyz, in matrix mvp)
{
    return mul(float4(xyz, 1.0), mvp);
}

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

OutVertex GetVertex(uint meshletIndex, uint vertexIndex, matrix mvp)
{
    float3 pos = Positions[vertexIndex];
    
    OutVertex v;
    v.pos = mul(float4(pos, 1.0), mvp);
    v.color = float3(
	    float(meshletIndex & 1),
	    float(meshletIndex & 3) / 4,
	    float(meshletIndex & 7) / 8
    );
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
        matrix mvp = mul(CbDrawData.MeshToWorld, mul(CbViewData.View, CbViewData.Projection));
        uint vertexIndex = GetVertexIndex(m, groupThreadID);
        outVerts[groupThreadID] = GetVertex(groupID, vertexIndex, mvp);
    }
    
    if (groupThreadID < m.PrimitiveCount)
    {
        outIndices[groupThreadID] = GetPrimitive(m, groupThreadID);
    }
}

float4 PSMain(OutVertex vertex) : SV_Target0
{
    return float4(vertex.color, 1.0);
}