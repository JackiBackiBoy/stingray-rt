#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "includes/bindless.glsl"
#include "includes/ray_payload.glsl"

layout (location = 0) rayPayloadInEXT RayPayload rayPayload;

void main() {
    rayPayload.color = vec3(0.0, 0.0, 0.0);
}