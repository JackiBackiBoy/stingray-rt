#ifndef SR_BINDLESS_H
#define SR_BINDLESS_H

#ifdef SR_VULKAN
	#define SR_VK_BINDING(_binding, _set) [[vk::binding(_binding, _set)]]
	#define SR_PUSH_CONSTANT(type, name) [[vk::push_constant]] type name;
#else
	#define SR_VK_BINDING(_binding, _set)
	#define SR_PUSH_CONSTANT(type, name) ConstantBuffer<type> name : register(b0, space0);
#endif

typedef uint SRDescriptorIndex;

struct PerFrameData {
	float4x4 view;
	float4x4 proj;
	float4x4 invView;
	float4x4 invProj;
};

// NOTE: We do not support bindless UBOs/CBVs, instead it's bindfull, so this
// is the only allowed constant buffer. We might add more in the future
SR_VK_BINDING(0, 1) ConstantBuffer<PerFrameData> g_PerFrameData : register(b1, space0);

// Textures (SRVs)
SR_VK_BINDING(0, 0) Texture2D<float4>            g_Texture2D[]  : register(t0, space1); 
SR_VK_BINDING(0, 0) Texture3D<float4>            g_Texture3D[]  : register(t0, space1);

// Samplers
// TODO: SamplerComparisonState probably needs its own definition?
SR_VK_BINDING(1, 0) SamplerState                 g_Sampler[]    : register(s0, space1);

#endif
