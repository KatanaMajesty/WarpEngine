struct ViewData
{
    matrix View;
    matrix Projection;
};

struct DrawData
{
    matrix InstanceToWorld;
};

ConstantBuffer<ViewData> CbViewData : register(b0);
ConstantBuffer<DrawData> CbDrawData : register(b1);

struct Meshlet
{
    uint VertexCount;
    uint VertexOffset;
    uint PrimitiveCount;
    uint PrimitiveOffset;
};

struct OutVertex
{
    float4 Pos : SV_Position;
};

StructuredBuffer<float3> Positions : register(t0, space0);
StructuredBuffer<Meshlet> Meshlets : register(t1);
ByteAddressBuffer UniqueVertexIndices : register(t2);
StructuredBuffer<uint> PrimitiveIndices : register(t3);

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
    
    OutVertex v;
    v.Pos = pos;
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