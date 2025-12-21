#include "Includes/Bindless.hlsl"

struct VSInput {
	float3 position : POSITION;
};

struct VSOutput {
	float4 position : SV_Position;
};

VSOutput vertexMain(VSInput input) {
	VSOutput output;
	output.position = mul(g_PerFrameData.view, float4(input.position, 1.0f));
	output.position = mul(g_PerFrameData.proj, output.position);
	return output;
}
