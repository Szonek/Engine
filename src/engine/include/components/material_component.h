#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
#define ENGINE_MATERIAL_USER_MAX_TEXTURE_BINDING_COUNT 8
#define ENGINE_MATERIAL_USER_MAX_UNIFORM_BUFFER_SIZE 512
typedef uint32_t engine_texture2d_t;
typedef uint32_t engine_shader_t;


typedef enum _engine_material_type_t
{
    ENGINE_MATERIAL_TYPE_PONG = 0,
    ENGINE_MATERIAL_TYPE_USER
} engine_material_type_t;

typedef struct _engine_material_pong_t
{
    float diffuse_color[4];
    engine_texture2d_t diffuse_texture;
    float shininess;
    engine_texture2d_t specular_texture;
} engine_material_pong_t;

typedef struct _engine_material_user_t
{
    engine_shader_t shader;
    uint8_t uniform_data_buffer[ENGINE_MATERIAL_USER_MAX_UNIFORM_BUFFER_SIZE];
    engine_texture2d_t texture_bindings[ENGINE_MATERIAL_USER_MAX_TEXTURE_BINDING_COUNT];
} engine_material_user_t;

typedef struct _engine_material_component_t
{
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