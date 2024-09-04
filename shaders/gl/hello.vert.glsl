#version 410

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;

layout (location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(inPosition.x, inPosition.y, inPosition.z, 1.0f);
    fragColor = inColor;
}
