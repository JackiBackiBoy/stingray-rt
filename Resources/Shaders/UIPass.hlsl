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
	float4 position                : SV_POSITION;
	float2 texcoord                : TEXCOORD0;
	nointerpolation float3 color   : COLOR;
	nointerpolation uint tex_index : TEXINDEX;
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

VSOutput vertex_main(uint VertexID : SV_VertexID, uint InstanceID : SV_InstanceID) {
	StructuredBuffer<UIDrawInstance> draw_instance_buffer = ResourceDescriptorHeap[g_Push.draw_intance_buffer_index];
	UIDrawInstance draw_instance = draw_instance_buffer[InstanceID];

	// TRICK: Bitwise trick to get corner offsets without vertex/index storage
    // X pattern: 0, 1, 1, 1, 0, 0 -> binary 001110 (0x0E)
    // Y pattern: 0, 0, 1, 1, 1, 0 -> binary 011100 (0x1C)
	float2 corner = float2(
		(float)((0x0EU >> VertexID) & 1U),
		(float)((0x1CU >> VertexID) & 1U)
	);

	VSOutput output;
	output.position = float4(draw_instance.pos + corner * draw_instance.size, 0.0f, 1.0f);
	output.position.x = (output.position.x * g_Push.inv_screen_width) * 2.0f - 1.0f;
	output.position.y = (output.position.y * g_Push.inv_screen_height) * 2.0f - 1.0f;
	output.position.y = -output.position.y;
	output.texcoord = draw_instance.texcoord_tl + corner * (draw_instance.texcoord_br - draw_instance.texcoord_tl);
	output.tex_index = draw_instance.tex_index;
	output.color = draw_instance.color;

	return output;
}

PSOutput pixel_main(VSOutput input) {
	if (input.tex_index == SR_INVALID_DESCRIPTOR_INDEX) {
		PSOutput output;
		output.color = float4(input.color, 1.0f);
		return output;
	}

	SamplerState sampler = SamplerDescriptorHeap[1]; // TODO: Remove hardcoded
	Texture2D<float> tex = ResourceDescriptorHeap[input.tex_index];
	float a = tex.Sample(sampler, input.texcoord).r;

	PSOutput output;
	output.color = float4(input.color, a);
	return output;
}
