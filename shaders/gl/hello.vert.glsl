#version 410 core

// Inputs
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inTangent;
layout (location = 3) in vec2 inTexCoord;

// Outputs
layout (location = 0) out vec3 fragNormal;
layout (location = 1) out vec2 fragTexCoord;

layout (std140) uniform PerFrameData {
	mat4 projectionMatrix;
	mat4 viewMatrix;
};

void main() {
    gl_Position =
		projectionMatrix *
		viewMatrix *
		vec4(inPosition.x, inPosition.y, inPosition.z, 1.0f);

	fragNormal = inNormal;
	fragTexCoord = inTexCoord;
}
