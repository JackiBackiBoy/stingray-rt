#version 450
#include "bindless.glsl"

// Inputs
layout (location = 0) in vec2 fsInTexCoord;

// Outputs
layout(location = 0) out vec4 fsOutColor;

void main() {
    fsOutColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
}