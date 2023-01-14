#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
#define ENGINE_ENTITY_NAME_MAX_LENGTH 128

typedef struct _engine_name_component_t
{
    char name[ENGINE_ENTITY_NAME_MAX_LENGTH];
} engine_name_component_t;

#ifdef __cplusplus
}
#endif // cpp