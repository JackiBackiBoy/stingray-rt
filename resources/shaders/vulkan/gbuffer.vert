#version 450
#include "bindless.glsl"

// VS Inputs
layout (location = 0) in vec3 vsInPosition;
layout (location = 1) in vec3 vsInNormal;
layout (location = 2) in vec3 vsInTangent;
layout (location = 3) in vec2 vsInTexCoord;

// VS Outputs
layout (location = 0) out vec3 vsOutWorldSpacePos;
layout (location = 1) out vec2 vsOutTexCoord;
layout (location = 2) out mat3 vsOutTBN;

layout (push_constant) uniform constants {
    mat4 modelMatrix;
    uint frameIndex;
    uint albedoMapIndex;
    uint normalMapIndex;
} g_PushConstants;

void main() {
    uint frameIndex = g_PushConstants.frameIndex;
    vec4 worldSpacePos = g_PushConstants.modelMatrix * vec4(vsInPosition, 1.0); // TODO: Add model matrix

    // TBN
    vec3 normTangent = normalize(vsInTangent);
    vec3 T = normalize(vec3(g_PushConstants.modelMatrix * vec4(vsInTangent, 0.0f)));
    vec3 N = normalize(vec3(g_PushConstants.modelMatrix * vec4(normalize(vsInNormal), 0.0f)));
    vec3 B = cross(N, T);

    vsOutWorldSpacePos = worldSpacePos.xyz;
    vsOutTexCoord = vsInTexCoord;
    vsOutTBN = mat3(T, B, N); // TODO: might not have to transpose

    gl_Position = g_PerFrameData[frameIndex].projectionMatrix * g_PerFrameData[frameIndex].viewMatrix * worldSpacePos;
}