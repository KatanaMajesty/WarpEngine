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

float4 TransformPosition(float x, float y, float z)
{
#if 0
    return mul(float4(x, y, z, 1.0), proj);
#else
    matrix viewProj = mul(view, proj);
    matrix mvp = mul(model, viewProj);
    return mul(float4(x, y, z, 1.0), mvp);
#endif
}

#define MAX_OUTPUT_VERTICES 256
#define MAX_OUTPUT_PRIMITIVES 256

[outputtopology("triangle")]
[numthreads(1, 1, 1)] // we are only using 1 thread, thus this is very bad for GPUs SIMD
void MSMain(
	out vertices Vertex outVerts[MAX_OUTPUT_VERTICES],
	out indices uint3 outIndices[MAX_OUTPUT_PRIMITIVES])
{
    const uint numVertices = 8;
    const uint numPrimitives = 12;
    SetMeshOutputCounts(numVertices, numPrimitives);
    
    outVerts[0].pos = TransformPosition(-0.5, -0.5, -0.5);
    outVerts[0].color = float3(0.0, 0.0, 0.0);
    
    outVerts[1].pos = TransformPosition(-0.5, -0.5, 0.5);
    outVerts[1].color = float3(0.0, 0.0, 1.0);
    
    outVerts[2].pos = TransformPosition(-0.5, 0.5, -0.5);
    outVerts[2].color = float3(0.0, 1.0, 0.0);
    
    outVerts[3].pos = TransformPosition(-0.5, 0.5, 0.5);
    outVerts[3].color = float3(0.0, 1.0, 1.0);
    
    outVerts[4].pos = TransformPosition(0.5, -0.5, -0.5);
    outVerts[4].color = float3(1.0, 0.0, 0.0);
    
    outVerts[5].pos = TransformPosition(0.5, -0.5, 0.5);
    outVerts[5].color = float3(1.0, 0.0, 1.0);
    
    outVerts[6].pos = TransformPosition(0.5, 0.5, -0.5);
    outVerts[6].color = float3(1.0, 1.0, 0.0);
    
    outVerts[7].pos = TransformPosition(0.5, 0.5, 0.5);
    outVerts[7].color = float3(1.0, 1.0, 1.0);
    
    outIndices[0] = uint3(0, 2, 1);
    outIndices[1] = uint3(1, 2, 3);
    outIndices[2] = uint3(4, 5, 6);
    outIndices[3] = uint3(5, 7, 6);
    outIndices[4] = uint3(0, 1, 5);
    outIndices[5] = uint3(0, 5, 4);
    outIndices[6] = uint3(2, 6, 7);
    outIndices[7] = uint3(2, 7, 3);
    outIndices[8] = uint3(0, 4, 6);
    outIndices[9] = uint3(0, 6, 2);
    outIndices[10] = uint3(1, 3, 7);
    outIndices[11] = uint3(1, 7, 5);
}

float4 PSMain(Vertex vertex) : SV_Target0
{
    return float4(vertex.color, 1.0);
}