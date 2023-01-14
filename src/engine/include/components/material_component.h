#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp

typedef struct _engine_material_component_t
{
    float ambient_color[4];
    float diffuse_color[4];
    float specular_color[4];
    float shiness;
} engine_material_component_t;

#ifdef __cplusplus
}
#endif // cpp