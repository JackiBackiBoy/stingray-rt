const uint MATERIAL_TYPE_LAMBERTIAN = 0;
const uint MATERIAL_TYPE_DIFFUSE_LIGHT = 1;
const uint MATERIAL_TYPE_METAL = 2;

struct Material {
    vec3 color;
    uint type;
    float roughness; // NOTE: Ranges [0, 1]
};