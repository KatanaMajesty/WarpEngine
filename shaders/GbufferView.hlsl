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

Texture2D ViewedGbuffer : register(t0, space0);
SamplerState StaticSampler : register(s0);

float4 PSMain(OutVertex vertex) : SV_Target0
{
    return ViewedGbuffer.SampleLevel(StaticSampler, vertex.Uv, 0.0);
}