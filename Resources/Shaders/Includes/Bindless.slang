#ifndef SR_BINDLESS_SLANG
#define SR_BINDLESS_SLANG

typedef uint SRDescriptorIndex;

struct PerFrameData {
    float4x4 view;
    float4x4 proj;
    float4x4 invView;
    float4x4 invProj;
};

#define SR_PUSH_CONSTANT(type, name) [[vk::push_constant]] cbuffer PushConstant##name : register(b0, space101) { type name; }

// Textures (SRVs)
[[vk::binding(0, 0)]] Texture2D<float4>            g_Texture2D[]  : register(t0, space0); 
[[vk::binding(0, 0)]] Texture3D<float4>            g_Texture3D[]  : register(t0, space0);

// Samplers
// TODO: SamplerComparisonState probably needs its own definition?
[[vk::binding(1, 0)]] SamplerState                 g_Sampler[]    : register(s0, space0);

// NOTE: We do not support bindless UBOs/CBVs, instead it's bindfull, so this
// is the only allowed constant buffer. We might add more in the future
[[vk::binding(0, 1)]] ConstantBuffer<PerFrameData> g_PerFrameData : register(b0, space1);

// TODO: Add the actual BINDLESS descriptors

#endif