#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
typedef uint32_t engine_texture2d_t;

typedef struct _engine_material_component_t
{
    float ambient_color[4];
    float diffuse_color[4];
    engine_texture2d_t diffuse_texture;
    float specular_color[4];
    engine_texture2d_t specular_texture;
    float shiness;
} engine_material_component_t;

#ifdef __cplusplus
}
#endif // cpp