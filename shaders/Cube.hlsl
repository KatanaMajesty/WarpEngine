cbuffer CubeCB : register(b0)
{
    matrix model;
    matrix view;
    matrix proj;
};

struct Vertex
{
    float4 pos : SV_Position;
    float3 color : COLOR0;
};

float4 TransformPosition(float3 xyz, in matrix mvp)
{
    return mul(float4(xyz, 1.0), mvp);
}

#define MAX_OUTPUT_VERTICES 256
#define MAX_OUTPUT_PRIMITIVES 256

static const float3 g_Positions[] = 
{
    float3(-0.5, -0.5, +0.5),
    float3(-0.5, -0.5, -0.5),
    float3(-0.5, 0.5, +0.5),
    float3(-0.5, 0.5, -0.5),
    float3(0.5, -0.5, +0.5),
    float3(0.5, -0.5, -0.5),
    float3(0.5, 0.5, +0.5),
    float3(0.5, 0.5, -0.5)
};

static const float3 g_Colors[] =
{
    float3(0.0, 0.0, 0.0),
    float3(0.0, 0.0, 1.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 1.0, 1.0),
    float3(1.0, 0.0, 0.0),
    float3(1.0, 0.0, 1.0),
    float3(1.0, 1.0, 0.0),
    float3(1.0, 1.0, 1.0),
};

static const uint3 g_Indices[] =
{
    uint3(0, 2, 1),
    uint3(1, 2, 3),
    uint3(4, 5, 6),
    uint3(5, 7, 6),
    uint3(0, 1, 5),
    uint3(0, 5, 4),
    uint3(2, 6, 7),
    uint3(2, 7, 3),
    uint3(0, 4, 6),
    uint3(0, 6, 2),
    uint3(1, 3, 7),
    uint3(1, 7, 5),
};

[outputtopology("triangle")]
[numthreads(12, 1, 1)]
void MSMain(
    in uint groupThreadID : SV_GroupThreadID,
	out vertices Vertex outVerts[MAX_OUTPUT_VERTICES],
	out indices uint3 outIndices[MAX_OUTPUT_PRIMITIVES])
{
    const uint numVertices = 8;
    const uint numPrimitives = 12;
    SetMeshOutputCounts(numVertices, numPrimitives);
    
    matrix mvp = mul(model, mul(view, proj));
    
    if (groupThreadID < numVertices)
    {
        outVerts[groupThreadID].pos = TransformPosition(g_Positions[groupThreadID], mvp);
        outVerts[groupThreadID].color = g_Colors[groupThreadID];
    }
    
    outIndices[groupThreadID] = g_Indices[groupThreadID];
}

float4 PSMain(Vertex vertex) : SV_Target0
{
    return float4(vertex.color, 1.0);
}