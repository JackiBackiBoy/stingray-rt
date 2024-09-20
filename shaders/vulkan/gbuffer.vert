#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inTangent;
layout (location = 3) in vec2 inTexCoord;

layout (location = 0) out vec3 vsOutWorldSpacePos;
layout (location = 1) out vec3 vsOutNormal;
layout (location = 2) out vec2 vsOutTexCoord;

// Global UBO
layout (set = 0, binding = 0) uniform PerFrameData {
    mat4 projectionMatrix;
    mat4 viewMatrix;
} g_PerFrameData[];

layout (push_constant) uniform constants {
    uint frameIndex;
} g_PushConstants;

void main() {
    uint frameIndex = g_PushConstants.frameIndex;
    vec4 worldSpacePos = vec4(inPosition, 1.0); // TODO: Add model matrix


    vsOutWorldSpacePos = worldSpacePos.xyz;
    vsOutNormal = inNormal; // TODO: TBN
    vsOutTexCoord = inTexCoord;
    gl_Position = g_PerFrameData[frameIndex].projectionMatrix * g_PerFrameData[frameIndex].viewMatrix * worldSpacePos;
}