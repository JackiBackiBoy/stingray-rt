#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;

// Global UBO
layout(set = 0, binding = 0) uniform PerFrameData {
    mat4 projectionMatrix;
    mat4 viewMatrix;
} g_PerFrameData[];

layout (push_constant) uniform constants {
    uint frameIndex;
} g_PushConstants;

void main() {
    uint frameIndex = g_PushConstants.frameIndex;
    gl_Position = g_PerFrameData[frameIndex].projectionMatrix * g_PerFrameData[frameIndex].viewMatrix * vec4(inPosition, 1.0);
}