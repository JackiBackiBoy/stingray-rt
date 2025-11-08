#include "Includes/Bindless.slang"

struct VSOutput {
	float4 position : SV_Position;
	float3 color    : COLOR0;
};

struct PSOutput {
	float4 color : SV_Target0;
};

VSOutput vertexMain(uint VertexID : SV_VertexID) {
	float2 positions[3] = {
		float2( 0.0f,  0.5f),
		float2(-0.5f, -0.5f),
		float2( 0.5f, -0.5f)
	};

	float3 colors[3] = {
		float3(1.0f, 0.0f, 0.0f), // Red
		float3(0.0f, 1.0f, 0.0f), // Green
		float3(0.0f, 0.0f, 1.0f)  // Blue
	};
	
	VSOutput output;
	output.position = mul(g_PerFrameData.view, float4(positions[VertexID], 0.0f, 1.0f));
	output.position = mul(g_PerFrameData.proj, output.position);
	output.color = colors[VertexID];
	return output;
}

PSOutput pixelMain(VSOutput input) {
	PSOutput output;
	output.color = float4(input.color, 1.0f);
	return output;
}
