#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
#include <stdint.h>

#define ENGINE_MAX_CHILDREN 8
typedef uint32_t engine_game_object_t;
typedef struct _engine_parent_component_t
{
    engine_game_object_t parent;
} engine_parent_component_t;

typedef struct _engine_children_component_t
{
    engine_game_object_t child[ENGINE_MAX_CHILDREN];
} engine_children_component_t;
#ifdef __cplusplus
}
#endif // cpp