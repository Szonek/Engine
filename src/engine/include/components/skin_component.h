#pragma once

#include "engine_string.h"

#ifdef __cplusplus
extern "C"
{
#endif // cpp

typedef struct _engine_skin_t engine_skin_t;
typedef struct _engine_skin_component_t
{
    engine_skin_t* skin;
} engine_skin_component_t;

// use joint attchment component to attach a game object to joint to follow it's position
typedef struct _engine_joint_attachment_component_t
{
    engine_skin_t* skin;
    engine_string_t* joint_name; // name of the joint to attach to, memory owned by engine, do not deallocate!
} engine_joint_attachment_component_t;

#ifdef __cplusplus
}
#endif // cpp
