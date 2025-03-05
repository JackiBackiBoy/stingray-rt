#extension GL_EXT_nonuniform_qualifier : require

#define BINDLESS_DESCRIPTOR_SET 0
#define BINDLESS_UBOS_BINDING 0
#define BINDLESS_TEXTURES_BINDING 1
#define BINDLESS_SAMPLERS_BINDING 2
#define BINDLESS_SSBOS_BINDING 3
#define BINDLESS_RW_TEXTURES_BINDING 4
#define BINDLESS_TLAS_BINDING 5 // NOTE: This isn't really "bindless", there will only be 1 descriptor at this binding (not an array)

// NOTE (Bindless SSBOs): We can interpret a single binding as multiple
// structure types. So it suffices to do the following:
// layout(set = 0, binding = 0, std140) buffer BulletBuffer { 
//      BulletInstance bulletInstances[]; 
// } storageBufferHeap[];
// I.e. Bindless SSBOs can just be interpreted on the fly

layout (set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_UBOS_BINDING) uniform PerFrameData {
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 invViewProjection;
    vec3 cameraPosition;
    uint pad1;
} g_PerFrameData[];
layout (set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_TEXTURES_BINDING) uniform texture2D g_Textures[];
layout (set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_SAMPLERS_BINDING) uniform sampler g_Samplers[];
layout (set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_RW_TEXTURES_BINDING, rgba8) uniform image2D g_RWTexturesRGBA8[];
layout (set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_RW_TEXTURES_BINDING, rgba16f) uniform image2D g_RWTexturesRGBA16f[];
layout (set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_RW_TEXTURES_BINDING, rgba32f) uniform image2D g_RWTexturesRGBA32f[];
layout (set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_RW_TEXTURES_BINDING, r32f) uniform image2D g_RWTexturesR32f[];
//layout (set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_TLAS_BINDING) uniform accelerationStructureEXT g_TLAS;