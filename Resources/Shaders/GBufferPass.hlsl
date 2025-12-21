#include "Includes/Bindless.hlsl"

struct VSInput {
	float3 position : POSITION;
};

struct VSOutput {
	float4 position : SV_Position;
};

struct PSOutput {
	float4 color : SV_Target0;
};

VSOutput vertexMain(VSInput input) {
	VSOutput output;
	output.position = mul(g_PerFrameData.view, float4(input.position, 1.0f));
	output.position = mul(g_PerFrameData.proj, output.position);
	return output;
}

PSOutput pixelMain(VSOutput input) {
	PSOutput output;
	output.color = float4(1.0f, 0.0f, 0.0f, 1.0f);
	return output;
}
