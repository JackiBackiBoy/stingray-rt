#version 450
#include "bindless.glsl"

const uint UI_TYPE_RECTANGLE = 0;
const uint UI_TYPE_TEXT = 1 << 0;

// Inputs
layout (location = 0) in vec2 fsInTexCoord;
layout (location = 1) flat in uint fsInTexIndex;
layout (location = 2) flat in uint fsInUIType;
layout (location = 3) in vec4 fsInColor;

// Outputs
layout(location = 0) out vec4 fsOutColor;

void main() {
    if (fsInUIType == UI_TYPE_RECTANGLE) {
        fsOutColor = fsInColor * vec4(texture(sampler2D(g_Textures[fsInTexIndex], g_Samplers[0]), fsInTexCoord).rgb, 1.0f);
    }
    else if (fsInUIType == UI_TYPE_TEXT) {
        fsOutColor = fsInColor * vec4(1.0f, 1.0f, 1.0f, texture(sampler2D(g_Textures[fsInTexIndex], g_Samplers[0]), fsInTexCoord).r);
    }
}