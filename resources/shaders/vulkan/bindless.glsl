#extension GL_EXT_nonuniform_qualifier : require

#define BINDLESS_DESCRIPTOR_SET 0
#define BINDLESS_UBOS_BINDING 0
#define BINDLESS_TEXTURES_BINDING 1

// NOTE: We can interpret a single binding as multiply
// structure types. So it suffices to do the following:
// layout(set = 0, binding = 0, std140) buffer BulletBuffer { 
//      BulletInstance bulletInstances[]; 
// } storageBufferHeap[];
// I.e. Bindless SSBOs can just be interpreted on the fly
#define BINDLESS_SSBOS_BINDING 3

layout (set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_UBOS_BINDING) uniform PerFrameData {
    mat4 projectionMatrix;
    mat4 viewMatrix;
} g_PerFrameData[];
layout (set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_TEXTURES_BINDING) uniform texture2D g_Textures[];
layout (set = BINDLESS_DESCRIPTOR_SET, binding = 2) uniform sampler g_Samplers[];