#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "includes/bindless.glsl"
#include "includes/ray_payload.glsl"

layout (location = 0) rayPayloadInEXT RayPayload rayPayload;

layout (push_constant) uniform constants {
    uint frameIndex;
    uint rtAccumulationIndex;
    uint rtImageIndex;
    uint sceneDescBufferIndex;
    uint rayBounces;
    uint samplesPerPixel;
    uint totalSamplesPerPixel;
    uint useNormalMaps;
    uint useSkybox;
} g_PushConstants;

void main() {
    if (g_PushConstants.useSkybox != 0) {
        const float t = 0.5 * (normalize(gl_WorldRayDirectionEXT).y + 1.0);
        const vec3 gradientStart = vec3(0.5, 0.6, 1.0);
        const vec3 gradientEnd = vec3(1.0);
        const vec3 skyColor = mix(gradientEnd, gradientStart, t);
        rayPayload.color = 3.0 * skyColor;
        rayPayload.distance = -1.0;
        return;
    }

    rayPayload.color = vec3(0.0);
    rayPayload.distance = -1.0;
}