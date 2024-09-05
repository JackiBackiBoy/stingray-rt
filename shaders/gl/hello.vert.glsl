#version 410 core

// Inputs
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;

// Outputs
layout (location = 0) out vec3 fragColor;

layout (std140) uniform PerFrameData {
	mat4 projectionMatrix;
	mat4 viewMatrix;
};

void main() {
    gl_Position =
		projectionMatrix *
		viewMatrix *
		vec4(inPosition.x, inPosition.y, inPosition.z, 1.0f);

    fragColor = inColor;
}
