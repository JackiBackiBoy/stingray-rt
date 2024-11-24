#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "includes/bindless.glsl"
#include "includes/ray_payload.glsl"

layout (location = 0) rayPayloadInEXT RayPayload rayPayload;

void main() {
    const float t = 0.5 * (normalize(gl_WorldRayDirectionEXT).y + 1.0);
    const vec3 gradientStart = vec3(0.5, 0.6, 1.0);
    const vec3 gradientEnd = vec3(1.0);
    const vec3 skyColor = mix(gradientEnd, gradientStart, t);
    rayPayload.color = skyColor;
}