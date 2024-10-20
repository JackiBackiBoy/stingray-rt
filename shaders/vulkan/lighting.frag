#version 450
#include "bindless.glsl"

// Inputs
layout (location = 0) in vec2 fsInTexCoord;

// Outputs
layout(location = 0) out vec4 fsOutColor;

layout (push_constant) uniform constants {
    uint positionIndex;
    uint albedoIndex;
    uint normalIndex;
} g_PushConstants;

const float CONSTANT_AMBIENT = 0.2f;

void main() {
    vec3 fragPos = texture(sampler2D(g_Textures[g_PushConstants.positionIndex], g_Samplers[0]), fsInTexCoord).rgb;
    vec3 albedo = texture(sampler2D(g_Textures[g_PushConstants.albedoIndex], g_Samplers[0]), fsInTexCoord).rgb;
    vec3 normal = normalize(texture(sampler2D(g_Textures[g_PushConstants.normalIndex], g_Samplers[0]), fsInTexCoord).rgb);    

    // Diffuse lighting
    vec3 dirToLight = normalize(vec3(0.5f, 1.0f, 0.6f));
    float diffuse = max(dot(normal, dirToLight), 0.0f);

    fsOutColor = vec4((diffuse + CONSTANT_AMBIENT) * albedo, 1.0f);
    //fsOutColor = vec4(normal, 1.0f);
}