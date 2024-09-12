#version 410 core

layout (location = 0) in vec3 fragNormal;
layout (location = 1) in vec2 fragTexCoord;

layout (location = 0) out vec4 outColor;

uniform sampler2D texture1;

void main() {
    outColor = texture(texture1, fragTexCoord);
}

