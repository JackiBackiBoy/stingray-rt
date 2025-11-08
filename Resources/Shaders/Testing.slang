#include "Includes/Bindless.slang"

struct VSInput {
	float3 position : POSITION;
	float2 texCoord : TEXCOORD0;
};

struct VSOutput {
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD0;
};

struct PSOutput {
	float4 color : SV_Target0;
};

VSOutput vertexMain(VSInput input) {
	VSOutput output;
	output.position = mul(g_PerFrameData.view, float4(input.position, 1.0f));
	output.position = mul(g_PerFrameData.proj, output.position);
	output.texCoord = input.texCoord;
	return output;
}

PSOutput pixelMain(VSOutput input) {
	PSOutput output;
	output.color = float4(input.texCoord, 0.0f, 1.0f);
	return output;
}
