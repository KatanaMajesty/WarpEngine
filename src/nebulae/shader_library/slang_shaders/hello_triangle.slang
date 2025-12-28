static float2 vertices[] = {
    float2(0.0f, -0.5f),
    float2(-0.5f, 0.5f),
    float2(0.5f, 0.5f),
};

struct VertexStageInput
{
    uint svVertexID : SV_VulkanVertexID;
};

struct VertexStageOutput
{
    [[vk::location(0)]] float3 color;
    float4 svPosition : SV_Position;
}

[shader("vertex")]
VertexStageOutput vertexMain(VertexStageInput input)
{
    float2 vertexPos = vertices[input.svVertexID];
    float3 color = float3(vertexPos.x + 0.5f, vertexPos.y + 0.5f, 1.0f - (vertexPos.x + 0.5f));

    VertexStageOutput output;
    output.color = color;
    output.svPosition = float4(vertexPos, 0.0f, 1.0f);
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexStageOutput input)
{
    return float4(input.color, 1.0);
}