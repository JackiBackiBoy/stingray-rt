#version 450
#include "bindless.glsl"

// FS Inputs
layout (location = 0) in vec3 fsInWorldSpacePos;
layout (location = 1) in vec3 fsInNormal;
layout (location = 2) in vec2 fsInTexCoord;
layout (location = 3) in mat3 fsInTBN;

// FS Outputs
layout (location = 0) out vec4 fsOutPosition;
layout (location = 1) out vec4 fsOutAlbedo;
layout (location = 2) out vec4 fsOutNormal;

layout (push_constant) uniform constants {
    mat4 modelMatrix;
    uint frameIndex;
    uint albedoMapIndex;
    uint normalMapIndex;
} g_PushConstants;

void main() {
    // Normal mapping with TBN
    vec3 surfaceNormal = texture(sampler2D(g_Textures[g_PushConstants.normalMapIndex], g_Samplers[0]), fsInTexCoord).rgb;
    surfaceNormal = normalize(surfaceNormal * 2.0f - 1.0f);
    surfaceNormal = normalize(vec3(fsInTBN * surfaceNormal));

    fsOutPosition = vec4(fsInWorldSpacePos, 1.0f);
    fsOutAlbedo = vec4(texture(sampler2D(g_Textures[g_PushConstants.albedoMapIndex], g_Samplers[0]), fsInTexCoord).rgb, 1.0f);
    fsOutNormal = vec4(surfaceNormal, 1.0f);
}