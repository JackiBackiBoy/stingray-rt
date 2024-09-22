#version 450
#include "bindless.glsl"

// Inputs
layout (location = 0) in vec2 fsInTexCoord;

// Outputs
layout(location = 0) out vec4 fsOutColor;

layout (push_constant) uniform constants {
    uint positionIndex;
    uint albedoIndex;
    uint normalIndex;
} g_PushConstants;

void main() {
    fsOutColor = vec4(texture(sampler2D(g_Textures[g_PushConstants.normalIndex], g_Samplers[0]), fsInTexCoord).rgb, 1.0f);
}