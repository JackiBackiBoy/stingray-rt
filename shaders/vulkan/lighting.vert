#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) out vec2 fragTexCoord;

// Global UBO
layout (set = 0, binding = 0) uniform PerFrameData {
    mat4 projectionMatrix;
    mat4 viewMatrix;
} g_PerFrameData[];

layout (set = 0, binding = 1) uniform texture2D g_Textures[];

void main() {
    fragTexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(fragTexCoord * vec2(2, -2) + vec2(-1, 1), 0.0f, 1.0f);
}