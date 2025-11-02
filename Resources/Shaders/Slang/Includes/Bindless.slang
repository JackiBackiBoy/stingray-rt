#ifndef SR_BINDLESS_SLANG
#define SR_BINDLESS_SLANG

typedef uint SRDescriptorIndex;

struct PerFrameData {
    float4x4 view;
    float4x4 proj;
    float4x4 invView;
    float4x4 invProj;
};

// NOTE: We do not support bindless UBOs/CBVs, instead it's bindfull, so this
// is the only allowed constant buffer. We might add more in the future
[[vk::binding(0, 1)]] ConstantBuffer<PerFrameData> g_PerFrameData : register(b0, space0);

// TODO: Add the actual BINDLESS descriptors

#endif