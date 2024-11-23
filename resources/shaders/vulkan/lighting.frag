#version 450
#extension GL_GOOGLE_include_directive : require

#include "includes/bindless.glsl"

// Inputs
layout (location = 0) in vec2 fsInTexCoord;

// Outputs
layout(location = 0) out vec4 fsOutColor;

layout (push_constant) uniform constants {
    uint positionIndex;
    uint albedoIndex;
    uint normalIndex;
    uint lightingUBOIndex;
} g_PushConstants;

const int MAX_CASCADES = 4;
const int MAX_POINT_LIGHTS = 32;

struct DirectionLight {
	mat4 cascadeProjections[MAX_CASCADES];
	mat4 viewMatrix;
	vec4 color; // NOTE: w is intensity
	vec3 direction;
	float cascadeDistances[MAX_CASCADES];
	uint numCascades;
};

struct PointLight {
	vec4 color; // NOTE: w-component is intensity
	vec3 position;
	uint pad1;
};

layout (set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_UBOS_BINDING) uniform LightingUBO {
    DirectionLight directionLight;
    uint numPointLights;
    uint pad1;
    uint pad2;
    uint pad3;
    PointLight pointLights[MAX_POINT_LIGHTS];
} g_LightingUBO[];

const float CONSTANT_AMBIENT = 0.2f;

void main() {
    DirectionLight directionLight = g_LightingUBO[g_PushConstants.lightingUBOIndex].directionLight;

    vec3 fragPos = texture(sampler2D(g_Textures[g_PushConstants.positionIndex], g_Samplers[0]), fsInTexCoord).rgb;
    vec3 albedo = texture(sampler2D(g_Textures[g_PushConstants.albedoIndex], g_Samplers[0]), fsInTexCoord).rgb;
    vec3 normal = normalize(texture(sampler2D(g_Textures[g_PushConstants.normalIndex], g_Samplers[0]), fsInTexCoord).rgb);    

    // Diffuse lighting
    vec3 dirToLight = normalize(directionLight.direction);
    float diffuse = max(dot(normal, dirToLight), 0.0f);

    fsOutColor = vec4((diffuse + CONSTANT_AMBIENT) * albedo, 1.0f);
    //fsOutColor = vec4(normal, 1.0f);
    //fsOutColor = vec4(directionLight.direction, 1.0f);
}