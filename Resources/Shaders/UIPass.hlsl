#include "Includes/Bindless.hlsl"

struct UIDrawInstance {
	float2 pos;
	float2 size;
	float2 texcoord_tl;
	float2 texcoord_br;
	float3 color;
	SRDescriptorIndex tex_index;
};

struct VSOutput {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD0;
	float3 color    : COLOR;
	uint tex_index  : TEXINDEX;
};

struct PSOutput {
	float4 color : SV_TARGET0;
};

struct PushConstants {
	float inv_screen_width;
	float inv_screen_height;
	SRDescriptorIndex draw_intance_buffer_index;
};

SR_PUSH_CONSTANT(PushConstants, g_Push);

static const float2 g_Vertices[4] = {
    float2(0.0f, 0.0f), // top left
    float2(1.0f, 0.0f), // top right
    float2(1.0f, 1.0f), // bottom right
    float2(0.0f, 1.0f) // bottom left
};

static const uint g_Indices[6] = {
    0, 1, 2, 2, 3, 0
};

VSOutput vertex_main(uint VertexID : SV_VertexID, uint InstanceID : SV_InstanceID) {
	StructuredBuffer<UIDrawInstance> draw_instance_buffer = ResourceDescriptorHeap[g_Push.draw_intance_buffer_index];
	UIDrawInstance draw_instance = draw_instance_buffer[InstanceID];
	uint vertex_index = g_Indices[VertexID];

	VSOutput output;
	output.position = float4(draw_instance.pos + g_Vertices[vertex_index] * draw_instance.size, 0.0f, 1.0f);
	output.position.x = (output.position.x * g_Push.inv_screen_width) * 2.0f - 1.0f;
	output.position.y = (output.position.y * g_Push.inv_screen_height) * 2.0f - 1.0f;
	output.position.y = -output.position.y;
	output.texcoord = draw_instance.texcoord_tl + g_Vertices[vertex_index] * (draw_instance.texcoord_br - draw_instance.texcoord_tl);
	output.tex_index = draw_instance.tex_index;
	output.color = draw_instance.color;

	return output;
}

PSOutput pixel_main(VSOutput input) {
	SamplerState sampler = SamplerDescriptorHeap[1]; // TODO: Remove hardcoded
	Texture2D<float> tex = ResourceDescriptorHeap[input.tex_index];
	float a = tex.Sample(sampler, input.texcoord).r;

	PSOutput output;
	output.color = float4(input.color, a);
	return output;
}
