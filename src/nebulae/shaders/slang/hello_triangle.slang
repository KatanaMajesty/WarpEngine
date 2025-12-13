/* Copyright (c) 2025, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

struct GlobalUniformBuffer
{
	float4x4 projectionMatrix;
	float4x4 modelMatrix;
	float4x4 viewMatrix;
};

[[vk::binding(0, 0)]] ConstantBuffer<GlobalUniformBuffer> ubo;

struct VertexStageInput
{
    [[vk::location(0)]] float3 position;
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
    output.svPosition = mul(ubo.projectionMatrix, mul(ubo.viewMatrix, mul(ubo.modelMatrix, float4(input.position, 1.0))));
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexStageOutput input)
{
    return float4(input.color, 1.0);
}