#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_GOOGLE_include_directive : require

#include "includes/bindless.glsl"
#include "includes/geometry_types.glsl"
#include "includes/material.glsl"
#include "includes/ray_payload.glsl"
#include "includes/ray_tracing_math.glsl"

struct Object {
	uint64_t verticesBDA;
	uint64_t indicesBDA;
	uint64_t materialsBDA;
};

layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; }; // Positions of an object
layout(buffer_reference, scalar) buffer Indices { uint i[]; }; // Triangle indices
layout(buffer_reference, scalar) buffer Materials { Material m[]; }; // Array of all materials on an object
layout (set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_SSBOS_BINDING) readonly buffer SceneDesc {
    Object objs[];
} g_SceneDesc[];

layout (push_constant) uniform constants {
    uint frameIndex;
    uint rtAccumulationIndex;
    uint rtImageIndex;
    uint sceneDescBufferIndex;
    uint samplesPerPixel;
    uint totalSamplesPerPixel;
} g_PushConstants;

layout (location = 0) rayPayloadInEXT RayPayload rayPayload;
hitAttributeEXT vec3 attribs;

// ----------------------------- Scatter Functions -----------------------------
RayPayload scatter_lambertian(Material mat, vec3 direction, vec3 normal, vec2 uv, float t, inout uint rngSeed) {
    RayPayload payload;
    payload.color = mat.color;
    payload.distance = t;
    payload.scatterDir = normal + RandomInUnitSphere(rngSeed);
    payload.isScattered = dot(direction, normal) < 0; // NOTE: Always true for Lambertian materials
    payload.rngSeed = rngSeed;

    return payload;
}

RayPayload scatter_diffuse_light(Material mat, float t, inout uint rngSeed) {
    RayPayload payload;
    payload.color = mat.color;
    payload.distance = t;
    payload.scatterDir = vec3(1, 0, 0);
    payload.isScattered = false; // Always false for diffuse light materials
    payload.rngSeed = rngSeed;

    return payload;
}

RayPayload scatter_metallic(Material mat, float t, inout uint rngSeed) {
    RayPayload payload;

    return payload;
}

RayPayload scatter(Material mat, vec3 dir, vec3 normal, vec2 uv, float t, inout uint rngSeed) {
    const vec3 normDir = normalize(dir);

    switch (mat.materialType) {
    case MATERIAL_TYPE_LAMBERTIAN:
        return scatter_lambertian(mat, normDir, normal, uv, t, rngSeed);
    case MATERIAL_TYPE_DIFFUSE_LIGHT:
        return scatter_diffuse_light(mat, t, rngSeed);
    }
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
    Vertex hitVtx = barycentric_lerp(vtx0, vtx1, vtx2, barycentrics);
    hitVtx.normal = normalize(vec3(hitVtx.normal * gl_WorldToObjectEXT));

    Materials mats = Materials(obj.materialsBDA);
    Material mat = mats.m[gl_InstanceCustomIndexEXT];

    // Scattering
    rayPayload = scatter(mat, gl_WorldRayDirectionEXT, hitVtx.normal, hitVtx.uv, gl_HitTEXT, rayPayload.rngSeed);
}