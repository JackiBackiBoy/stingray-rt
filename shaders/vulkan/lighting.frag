#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout (set = 0, binding = 1) uniform texture2D g_Textures[];
layout (set = 0, binding = 2) uniform sampler g_Samplers[];

void main() {
    outColor = vec4(texture(sampler2D(g_Textures[1], g_Samplers[0]), fragTexCoord).rgb, 1.0f);
}