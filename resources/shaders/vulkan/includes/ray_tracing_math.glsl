// --------------------------------- Constants ---------------------------------
const float PI = 3.141592653589793;
const float PI2 = 6.283185307179586;
const float PI_HALF = 1.5707963267948966;

// -------------------------- Random Number Functions --------------------------
uint step_rng(uint rngSeed) {
    return rngSeed * 747796405 + 1;
}

// Steps the RNG and returns a floating-point value between 0 and 1 inclusive.
float rand_float(inout uint rngSeed) {
    rngSeed = step_rng(rngSeed);
    uint word = ((rngSeed >> ((rngSeed >> 28) + 4)) ^ rngSeed) * 277803737;
    word = (word >> 22) ^ word;
    return float(word) / 4294967295.0f;
}

vec2 rand_vec2(inout uint rngSeed) {
    return vec2(rand_float(rngSeed), rand_float(rngSeed));
}

vec3 rand_cos_hemisphere_dir(inout uint rngSeed) {
    float r = sqrt(rand_float(rngSeed));
    float theta = 2.0 * PI * rand_float(rngSeed);
    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(max(0.0, 1.0 - x*x - y*y));

    return vec3(x, y, z);
}

// --------------------------- Barycentric Functions ---------------------------
float barycentric_lerp(float v0, float v1, float v2, vec3 barycentrics) {
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

vec2 barycentric_lerp(vec2 v0, vec2 v1, vec2 v2, vec3 barycentrics) {
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

vec3 barycentric_lerp(vec3 v0, vec3 v1, vec3 v2, vec3 barycentrics) {
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

vec4 barycentric_lerp(vec4 v0, vec4 v1, vec4 v2, vec3 barycentrics) {
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

Vertex barycentric_lerp(Vertex v0, Vertex v1, Vertex v2, vec3 barycentrics) {
    Vertex vtx;
    vtx.pos = barycentric_lerp(v0.pos, v1.pos, v2.pos, barycentrics);
    vtx.normal = normalize(barycentric_lerp(v0.normal, v1.normal, v2.normal, barycentrics));
    vtx.tangent = normalize(barycentric_lerp(v0.tangent, v1.tangent, v2.tangent, barycentrics));
    vtx.uv = barycentric_lerp(v0.uv, v1.uv, v2.uv, barycentrics);

    return vtx;
}