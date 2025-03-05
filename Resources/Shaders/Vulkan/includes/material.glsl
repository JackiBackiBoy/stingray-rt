const uint MATERIAL_TYPE_NOT_DIFFUSE_LIGHT = 0;
const uint MATERIAL_TYPE_DIFFUSE_LIGHT = 1;

struct Material {
    vec3 color;
    uint type;
    uint albedoTexIndex;
    uint normalTexIndex;
    float metallic;
    float roughness; // NOTE: Ranges [0, 1]
    float ior;
};