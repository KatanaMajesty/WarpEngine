struct VertexStageInput
{
    [[vk::location(0)]] float2 position;
    [[vk::location(1)]] float3 color;
};

struct VertexStageOutput
{
    [[vk::location(0)]] float3 color;
    float4 svPosition : SV_Position;
}

[shader("vertex")]
VertexStageOutput vertexMain(VertexStageInput input)
{
    VertexStageOutput output;
    output.color = input.color;
    output.svPosition = float4(input.position, 0.0f, 1.0f);
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexStageOutput input)
{
    return float4(input.color, 1.0);
}