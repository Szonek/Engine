#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
#define ENGINE_FOLDER_PATH_MAX_LENGTH 128

// Where the game object is listed in the editor's scene hierarchy: a path of folder names separated by '/', e.g.
// "environment/walls". It only organizes the hierarchy, it has no other effect (unlike a parent component). It applies
// to game objects without a parent; a child is listed under its parent. An empty path means the hierarchy's root.
typedef struct _engine_folder_component_t
{
    char path[ENGINE_FOLDER_PATH_MAX_LENGTH];
} engine_folder_component_t;

#ifdef __cplusplus
}
#endif // cpp
