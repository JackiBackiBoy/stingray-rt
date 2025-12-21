#include "Includes/Bindless.hlsl"

struct PushConstants {
	uint albedoTexIndex;
};

struct VSOutput {
	float4 position : SV_Position;
	float2 uv : TEXCOORD;
};

struct PSOutput {
	float4 color : SV_Target0;
};

SR_PUSH_CONSTANT(PushConstants, g_Push);

VSOutput vertexMain(uint VertexID : SV_VertexID) {
	// TRICK: Fullscreen triangle
	VSOutput output;
	output.uv.x = (VertexID << 1) & 2;
	output.uv.y = VertexID & 2;
	output.position = float4(output.uv * float2(2, -2) + float2(-1, 1), 0, 1);
	return output;
}

PSOutput pixelMain(VSOutput input) {
	SamplerState sampler = SamplerDescriptorHeap[0]; // TODO: Remove hardcoded
	Texture2D<float4> albedoTex = ResourceDescriptorHeap[g_Push.albedoTexIndex];
	float4 albedo = albedoTex.Sample(sampler, input.uv).rgba;

	PSOutput output;
	output.color = albedo;
	return output;
}
