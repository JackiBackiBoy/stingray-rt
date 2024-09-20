#version 450
#extension GL_EXT_nonuniform_qualifier : require

// Inputs
layout (location = 0) in vec3 vsOutWorldSpacePos;
layout (location = 1) in vec3 vsOutNormal;
layout (location = 2) in vec2 vsOutTexCoord;

// Outputs
layout (location = 0) out vec4 fsOutPosition;
layout (location = 1) out vec4 fsOutAlbedo;
layout (location = 2) out vec4 fsOutNormal;

// Descriptors
layout (set = 0, binding = 1) uniform texture2D g_Textures[];
layout (set = 0, binding = 2) uniform sampler g_Samplers[];

void main() {
    vec3 surfaceNormal = vsOutNormal * 2.0f - 1.0f;

    fsOutPosition = vec4(vsOutWorldSpacePos, 1.0f);
    fsOutAlbedo = vec4(texture(sampler2D(g_Textures[0], g_Samplers[0]), vsOutTexCoord).rgb, 1.0f);
    fsOutNormal = vec4(surfaceNormal, 1.0f);
}