#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
#define ENGINE_MATERIAL_CUSTOM_MAX_TEXTURE_BINDING_COUNT 8
typedef uint32_t engine_material_t;
typedef uint32_t engine_texture2d_t;
typedef uint32_t engine_shader_t;


typedef enum _engine_material_type_t
{
    ENGINE_MATERIAL_TYPE_PONG = 0,
    ENGINE_MATERIAL_TYPE_DEFAULT
} engine_material_type_t;

typedef struct _engine_material_pong_t
{
    float diffuse_color[4];
    engine_texture2d_t diffuse_texture;
    uint32_t shininess;
    engine_texture2d_t specular_texture;
} engine_material_pong_t;

typedef struct _engine_material_user_t
{
    engine_shader_t shader;
    const void* uniform_buffer_data;
    uint32_t uniform_buffer_size;
    engine_texture2d_t texture_bindings[ENGINE_MATERIAL_CUSTOM_MAX_TEXTURE_BINDING_COUNT];
} engine_material_user_t;

typedef struct _engine_material_component_t
{
    engine_material_t material;
    engine_material_type_t type;
    union
    {
        engine_material_pong_t pong;
        engine_material_user_t user;
    } data;


} engine_material_component_t;

#ifdef __cplusplus
}
#endif // cpp