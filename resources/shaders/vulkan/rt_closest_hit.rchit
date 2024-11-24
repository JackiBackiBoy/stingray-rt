#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_GOOGLE_include_directive : require

#include "includes/bindless.glsl"
#include "includes/geometry_types.glsl"
#include "includes/ray_payload.glsl"

struct Object {
	uint64_t verticesBDA;
	uint64_t indicesBDA;
	uint64_t materialsBDA;
};

struct Material {
    vec3 color;
    float refractiveIndex;
};

layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; }; // Positions of an object
layout(buffer_reference, scalar) buffer Indices { uint i[]; }; // Triangle indices
layout(buffer_reference, scalar) buffer Materials { Material m[]; }; // Array of all materials on an object
layout (set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_SSBOS_BINDING) readonly buffer SceneDesc {
    Object objs[];
} g_SceneDesc[];

layout (push_constant) uniform constants {
    uint frameIndex;
    uint rtImageIndex;
    uint sceneDescBufferIndex;
} g_PushConstants;

layout (location = 0) rayPayloadInEXT RayPayload rayPayload;
hitAttributeEXT vec3 attribs;

float barycentric_lerp(in float v0, in float v1, in float v2, in vec3 barycentrics) {
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

vec2 barycentric_lerp(in vec2 v0, in vec2 v1, in vec2 v2, in vec3 barycentrics) {
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

vec3 barycentric_lerp(in vec3 v0, in vec3 v1, in vec3 v2, in vec3 barycentrics) {
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

vec4 barycentric_lerp(in vec4 v0, in vec4 v1, in vec4 v2, in vec3 barycentrics) {
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

Vertex barycentric_lerp(in Vertex v0, in Vertex v1, in Vertex v2, in vec3 barycentrics) {
    Vertex vtx;
    vtx.pos = barycentric_lerp(v0.pos, v1.pos, v2.pos, barycentrics);
    vtx.normal = normalize(barycentric_lerp(v0.normal, v1.normal, v2.normal, barycentrics));
    vtx.tangent = normalize(barycentric_lerp(v0.tangent, v1.tangent, v2.tangent, barycentrics));
    vtx.uv = barycentric_lerp(v0.uv, v1.uv, v2.uv, barycentrics);

    return vtx;
}

void main() {
    Object obj = g_SceneDesc[g_PushConstants.sceneDescBufferIndex].objs[gl_InstanceCustomIndexEXT];
    Vertices vertices = Vertices(obj.verticesBDA);
    Indices indices = Indices(obj.indicesBDA);

    // Triangle
    Vertex vtx0 = vertices.v[indices.i[gl_PrimitiveID * 3]];
    Vertex vtx1 = vertices.v[indices.i[gl_PrimitiveID * 3 + 1]];
    Vertex vtx2 = vertices.v[indices.i[gl_PrimitiveID * 3 + 2]];

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    const Vertex hitVertex = barycentric_lerp(vtx0, vtx1, vtx2, barycentrics);

    Materials mats = Materials(obj.materialsBDA);
    Material mat = mats.m[gl_InstanceCustomIndexEXT];

    rayPayload.color = mat.color;
}