#version 450
#include "bindless.glsl"

// VS Inputs
layout (location = 0) in vec3 vsInPosition;
layout (location = 1) in vec3 vsInNormal;
layout (location = 2) in vec3 vsInTangent;
layout (location = 3) in vec2 vsInTexCoord;

// VS Outputs
layout (location = 0) out vec3 vsOutWorldSpacePos;
layout (location = 1) out vec3 vsOutNormal;
layout (location = 2) out vec2 vsOutTexCoord;

layout (push_constant) uniform constants {
    uint frameIndex;
} g_PushConstants;

void main() {
    uint frameIndex = g_PushConstants.frameIndex;
    vec4 worldSpacePos = vec4(vsInPosition, 1.0); // TODO: Add model matrix

    vsOutWorldSpacePos = worldSpacePos.xyz;
    vsOutNormal = vsInNormal; // TODO: TBN
    vsOutTexCoord = vsInTexCoord;
    gl_Position = g_PerFrameData[frameIndex].projectionMatrix * g_PerFrameData[frameIndex].viewMatrix * worldSpacePos;
}