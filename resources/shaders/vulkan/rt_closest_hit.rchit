#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_GOOGLE_include_directive : require

#include "includes/bindless.glsl"
#include "includes/ray_payload.glsl"

layout(buffer_reference, scalar) buffer Vertices {vec4 v[]; }; // Positions of an object

layout (location = 0) rayPayloadInEXT RayPayload rayPayload;

void main() {
    rayPayload.color = vec3(1.0, 1.0, 1.0);
}