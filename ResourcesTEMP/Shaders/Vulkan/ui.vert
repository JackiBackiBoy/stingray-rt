#version 450
#extension GL_GOOGLE_include_directive : require

#include "includes/bindless.glsl"

// Outputs
layout (location = 0) out vec2 vsOutTexCoord;
layout (location = 1) out uint vsOutTexIndex;
layout (location = 2) out uint vsOutUIType;
layout (location = 3) out vec4 vsOutColor;

struct UIParams {
    vec4 color;
    vec2 position;
    vec2 size;
    vec2 texCoords[4];
    uint texIndex;
    uint uiType;
    uint pad2;
    uint pad3;
};

layout (set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_SSBOS_BINDING) readonly buffer UIParamsBuffer {
    UIParams params[];
} g_UISSBO[];

layout (push_constant) uniform constants {
    mat4 projectionMatrix;
    uint uiParamsBufferIndex;
} g_PushConstants;

const vec2 g_Vertices[4] = {
    vec2(0.0f, 0.0f), // top left
    vec2(1.0f, 0.0f), // top right
    vec2(1.0f, 1.0f), // bottom right
    vec2(0.0f, 1.0f) // bottom left
};

const uint g_Indices[6] = {
    0, 1, 2, 2, 3, 0
};

void main() {
    UIParams params = g_UISSBO[g_PushConstants.uiParamsBufferIndex].params[gl_InstanceIndex];
    uint vertexIndex = g_Indices[gl_VertexIndex];
    vec2 vertex = g_Vertices[vertexIndex];

    vsOutTexCoord = params.texCoords[vertexIndex];
    vsOutTexIndex = params.texIndex;
    vsOutUIType = params.uiType;
    vsOutColor = params.color;

    gl_Position = g_PushConstants.projectionMatrix * vec4(
        vertex.x * params.size.x + params.position.x,
        vertex.y * params.size.y + params.position.y,
        0.0,
        1.0
    );
}