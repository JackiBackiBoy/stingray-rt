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
    uint64_t pad1;
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
float schlick_fresnel(float cosTheta, float ior) {
    float r0 = (1.0 - ior) / (1.0 + ior);
    r0 *= r0;
    return r0 + (1.0 - r0) * pow(1.0 - cosTheta, 5);
}

RayPayload scatter_combined(Material mat, vec3 dir, vec3 normal, vec2 uv, float t, inout uint rngSeed) {
    // dot(u, v) = ||u|| * ||v|| * cos(theta)
    // if ||u|| = ||v|| = 1 => dot(u, v) = cos(theta)
    // NOTE: We assume `dir` and `normal` to be normalized
    float cosTheta = dot(-dir, normal);
    float fresnel = mix(schlick_fresnel(cosTheta, mat.ior), 1.0, mat.metallic);

    vec3 diffuseDir = normal + RandomInUnitSphere(rngSeed);
    vec3 reflectDir = reflect(dir, normal);

    bool isSpecular = RandomFloat(rngSeed) < fresnel;
    vec3 scatterDir = isSpecular ? mix(reflectDir, diffuseDir, mat.roughness) : diffuseDir;

    // if (cosTheta < 0) {
    //     scatterDir = -scatterDir;
    // }

    vec3 albedoTexColor = texture(sampler2D(g_Textures[mat.albedoTexIndex], g_Samplers[0]), uv).rgb;

    RayPayload payload;
    payload.color = mat.color * albedoTexColor;
    payload.distance = t;
    payload.scatterDir = scatterDir;
    payload.isScattered = true;
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

RayPayload scatter(Material mat, vec3 dir, vec3 normal, vec2 uv, float t, inout uint rngSeed) {
    const vec3 normDir = normalize(dir);

    switch (mat.type) {
    case MATERIAL_TYPE_NOT_DIFFUSE_LIGHT:
        return scatter_combined(mat, normDir, normal, uv, t, rngSeed);
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
    Material mat = mats.m[hitVtx.matIndex];

    // Normal mapping
    vec3 T = normalize(vec3(hitVtx.tangent * gl_WorldToObjectEXT));
    vec3 N = hitVtx.normal;
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);
    hitVtx.normal = TBN * normalize(
        texture(sampler2D(g_Textures[mat.normalTexIndex], g_Samplers[0]), hitVtx.uv).rgb * 2.0 - 1.0
    );

    // Scattering
    rayPayload = scatter(mat, gl_WorldRayDirectionEXT, hitVtx.normal, hitVtx.uv, gl_HitTEXT, rayPayload.rngSeed);
}