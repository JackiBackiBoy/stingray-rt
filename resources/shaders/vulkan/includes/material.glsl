const uint MATERIAL_TYPE_LAMBERTIAN = 0;
const uint MATERIAL_TYPE_DIFFUSE_LIGHT = 1;

struct Material {
    vec3 color;
    uint materialType;
};