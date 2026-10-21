#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
typedef uint32_t engine_texture2d_t;


typedef enum _engine_material_type_t
{
    ENGINE_MATERIAL_TYPE_PONG = 0,
} engine_material_type_t;

typedef struct _engine_material_pong_t
{
    float diffuse_color[4];
    engine_texture2d_t diffuse_texture;
    float shininess;
    engine_texture2d_t specular_texture;
} engine_material_pong_t;

typedef struct _engine_material_component_t
{
    engine_material_type_t type;
    union
    {
        engine_material_pong_t pong;
    } data;


} engine_material_component_t;

#ifdef __cplusplus
}
#endif // cpp