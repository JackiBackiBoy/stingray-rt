#include "Includes/Bindless.hlsl"

struct VSOutput {
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD0;
};

struct PSOutput {
	float4 color : SV_TARGET0;
};

struct PushConstants {
	SRDescriptorIndex font_atlas_tex_index;
};

SR_PUSH_CONSTANT(PushConstants, g_Push);

static const float2 vertices[4] = {
	float2(-0.5f, -0.5f),
	float2( 0.5f, -0.5f),
	float2(-0.5f,  0.5f),
	float2( 0.5f,  0.5f)
};
static const uint indices[6] = {
	0, 1, 2,
	2, 1, 3
};
static const float2 tex_coords[4] = {
	float2(0.0f, 1.0f),
	float2(1.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f)
};

VSOutput vertex_main(uint VertexID : SV_VertexID) {
	VSOutput output;
	output.texCoord.x = tex_coords[indices[VertexID]].x;
	output.texCoord.y = tex_coords[indices[VertexID]].y;
	output.position = float4(vertices[indices[VertexID]], 0.0f, 1.0f);
	return output;
}

PSOutput pixel_main(VSOutput input) {
	SamplerState sampler = SamplerDescriptorHeap[1]; // TODO: Remove hardcoded
	Texture2D<float> font_atlas_tex = ResourceDescriptorHeap[g_Push.font_atlas_tex_index];
	float4 color = font_atlas_tex.Sample(sampler, input.texCoord).r;

	PSOutput output;
	output.color = float4(color.r, 0.0f, 0.0f, 1.0f);
	return output;
}
