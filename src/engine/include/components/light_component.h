#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp

typedef struct _engine_light_intensity_t
{
    float ambient[3];
    float diffuse[3];
    float specular[3];
} engine_light_intensity_t;

typedef enum _engine_light_type_t
{
    ENGINE_LIGHT_TYPE_DIRECTIONAL = 0,
    ENGINE_LIGHT_TYPE_POINT = 1,
    ENGINE_LIGHT_TYPE_SPOT = 2,
} engine_light_type_t;

typedef struct _engine_directional_light_data_t
{
    float direction[3];
} engine_directional_light_data_t;

typedef struct _engine_point_light_data_t
{
    float constant;
    float linear;
    float quadratic;
} engine_point_light_data_t;

typedef struct _engine_spot_light_data_t
{
    float direction[3];
    float cut_off;  // degrees
    float outer_cut_off; // degrees
    float constant;
    float linear;
    float quadratic;
} engine_spot_light_data_t;


typedef struct _engine_light_component_t
{
    engine_light_type_t type;
    engine_light_intensity_t intensity;
    union
    {
        engine_directional_light_data_t directional;
        engine_point_light_data_t point;
        engine_spot_light_data_t spot;
    };
} engine_light_component_t;

#ifdef __cplusplus
}
#endif // cpp