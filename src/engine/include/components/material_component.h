#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
typedef uint32_t engine_texture2d_t;


// Phong shading parameters, the only kind of material for now (a material system will add others).
typedef struct _engine_material_component_t
{
    float diffuse_color[4];
    engine_texture2d_t diffuse_texture;
    float shininess;
    engine_texture2d_t specular_texture;
} engine_material_component_t;

#ifdef __cplusplus
}
#endif // cpp