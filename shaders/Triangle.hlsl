struct VSOutput
{
	float4 position : SV_Position;
	float4 color : COLOR;
};

VSOutput vs_main(float3 pos : POSITION, float4 color : COLOR)
{
	VSOutput output;
	output.position = float4(pos, 1.0);
	output.color = color;
	return output;
}

float4 ps_main(in VSOutput input) : SV_Target0
{
	return input.color;
}