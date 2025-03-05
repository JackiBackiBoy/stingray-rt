#version 450
#extension GL_GOOGLE_include_directive : require

#include "includes/bindless.glsl"

// Inputs
layout (location = 0) in vec2 fsInTexCoord;

// Outputs
layout(location = 0) out vec4 fsOutColor;

layout (push_constant) uniform constants {
    uint texIndex;
} g_PushConstants;

void main() {
    //fsOutColor = vec4(1.0, 0.0, 0.0, 1.0);
    fsOutColor = vec4(texture(sampler2D(g_Textures[g_PushConstants.texIndex], g_Samplers[0]), fsInTexCoord).rgb, 1.0);
}