#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_GOOGLE_include_directive : require

#include "includes/bindless.glsl"
#include "includes/geometry_types.glsl"
#include "includes/ray_payload.glsl"
#include "includes/ray_tracing_math.glsl"

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

RayPayload scatter(Vertex vtx, Material mat, uint rngSeed) {
    RayPayload payload;
    payload.distance = gl_HitTEXT;

    payload.color = mat.color;
    payload.scatterDir = vtx.normal + rand_cos_hemisphere_dir(rngSeed);
    payload.rngSeed = rngSeed;

    return payload;
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

    rayPayload = scatter(hitVtx, mat, rayPayload.rngSeed);
}