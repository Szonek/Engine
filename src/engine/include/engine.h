#pragma once

#ifdef engine_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif

#define ENGINE_BIT_MASK(id) 1 << id
#define ENGINE_BIT_MASK_MAX 0x7FFFFFFF

#define ENGINE_APPLICATION_NAME_MAX_LENGTH 256

#ifdef __cplusplus
extern "C"
{
#endif  // #ifndef __cplusplus

#include "components/camera_component.h"
#include "components/light_component.h"
#include "components/transform_component.h"
#include "components/name_component.h"
#include "components/mesh_component.h"
#include "components/material_component.h"

#include <stdint.h>

typedef uint32_t engine_game_object_t;
typedef struct _engine_application_t* engine_application_t;
typedef struct _engine_scene_t* engine_scene_t;
typedef uint32_t engine_texture2d_t;
typedef uint32_t engine_geometry_t;

typedef struct _engine_application_create_desc_t
{
    const char* name;
    const char* asset_store_path;
    uint32_t width;
    uint32_t height;
} engine_application_create_desc_t;

typedef struct _engine_application_frame_begine_info_t
{
    float delta_time;
} engine_application_frame_begine_info_t;


typedef struct _engine_application_frame_end_info_t
{
    bool success;
} engine_application_frame_end_info_t;

typedef struct _engine_mouse_coords_t
{
    int32_t x;
    int32_t y;
} engine_mouse_coords_t;

typedef enum _engine_mouse_button_t
{
    ENGINE_MOUSE_BUTTON_1        =  0,
    ENGINE_MOUSE_BUTTON_2        =  1,
    ENGINE_MOUSE_BUTTON_3        =  2,
    ENGINE_MOUSE_BUTTON_4        =  3,
    ENGINE_MOUSE_BUTTON_5        =  4,
    ENGINE_MOUSE_BUTTON_6        =  5,
    ENGINE_MOUSE_BUTTON_7        =  6,
    ENGINE_MOUSE_BUTTON_8        =  7,
    ENGINE_MOUSE_BUTTON_LAST     = ENGINE_MOUSE_BUTTON_8,
    ENGINE_MOUSE_BUTTON_LEFT     = ENGINE_MOUSE_BUTTON_1,
    ENGINE_MOUSE_BUTTON_RIGHT    = ENGINE_MOUSE_BUTTON_2,
    ENGINE_MOUSE_BUTTON_MIDDLE   = ENGINE_MOUSE_BUTTON_3,
} engine_mouse_button_t;

typedef enum _engine_platform_keys_t
{
    ENGINE_KEYBOARD_KEY_SPACE              = 32,
    ENGINE_KEYBOARD_KEY_APOSTROPHE         = 39,  /* ' */
    ENGINE_KEYBOARD_KEY_COMMA              = 44,  /* , */
    ENGINE_KEYBOARD_KEY_MINUS              = 45,  /* - */
    ENGINE_KEYBOARD_KEY_PERIOD             = 46,  /* . */
    ENGINE_KEYBOARD_KEY_SLASH              = 47,  /* / */
    ENGINE_KEYBOARD_KEY_0                  = 48,
    ENGINE_KEYBOARD_KEY_1                  = 49,
    ENGINE_KEYBOARD_KEY_2                  = 50,
    ENGINE_KEYBOARD_KEY_3                  = 51,
    ENGINE_KEYBOARD_KEY_4                  = 52,
    ENGINE_KEYBOARD_KEY_5                  = 53,
    ENGINE_KEYBOARD_KEY_6                  = 54,
    ENGINE_KEYBOARD_KEY_7                  = 55,
    ENGINE_KEYBOARD_KEY_8                  = 56,
    ENGINE_KEYBOARD_KEY_9                  = 57,
    ENGINE_KEYBOARD_KEY_SEMICOLON          = 59,  /* ; */
    ENGINE_KEYBOARD_KEY_EQUAL              = 61,  /* = */
    ENGINE_KEYBOARD_KEY_A                  = 65,
    ENGINE_KEYBOARD_KEY_B                  = 66,
    ENGINE_KEYBOARD_KEY_C                  = 67,
    ENGINE_KEYBOARD_KEY_D                  = 68,
    ENGINE_KEYBOARD_KEY_E                  = 69,
    ENGINE_KEYBOARD_KEY_F                  = 70,
    ENGINE_KEYBOARD_KEY_G                  = 71,
    ENGINE_KEYBOARD_KEY_H                  = 72,
    ENGINE_KEYBOARD_KEY_I                  = 73,
    ENGINE_KEYBOARD_KEY_J                  = 74,
    ENGINE_KEYBOARD_KEY_K                  = 75,
    ENGINE_KEYBOARD_KEY_L                  = 76,
    ENGINE_KEYBOARD_KEY_M                  = 77,
    ENGINE_KEYBOARD_KEY_N                  = 78,
    ENGINE_KEYBOARD_KEY_O                  = 79,
    ENGINE_KEYBOARD_KEY_P                  = 80,
    ENGINE_KEYBOARD_KEY_Q                  = 81,
    ENGINE_KEYBOARD_KEY_R                  = 82,
    ENGINE_KEYBOARD_KEY_S                  = 83,
    ENGINE_KEYBOARD_KEY_T                  = 84,
    ENGINE_KEYBOARD_KEY_U                  = 85,
    ENGINE_KEYBOARD_KEY_V                  = 86,
    ENGINE_KEYBOARD_KEY_W                  = 87,
    ENGINE_KEYBOARD_KEY_X                  = 88,
    ENGINE_KEYBOARD_KEY_Y                  = 89,
    ENGINE_KEYBOARD_KEY_Z                  = 90,
    ENGINE_KEYBOARD_KEY_LEFT_BRACKET       = 91,  /* [ */
    ENGINE_KEYBOARD_KEY_BACKSLASH          = 92,  /* \ */
    ENGINE_KEYBOARD_KEY_RIGHT_BRACKET      = 93,  /* ] */
    ENGINE_KEYBOARD_KEY_GRAVE_ACCENT       = 96,  /* ` */
    ENGINE_KEYBOARD_KEY_WORLD_1            = 161, /* non-US #1 */
    ENGINE_KEYBOARD_KEY_WORLD_2            = 162, /* non-US #2 */
    ENGINE_KEYBOARD_KEY_ESCAPE             = 256,
    ENGINE_KEYBOARD_KEY_ENTER              = 257,
    ENGINE_KEYBOARD_KEY_TAB                = 258,
    ENGINE_KEYBOARD_KEY_BACKSPACE          = 259,
    ENGINE_KEYBOARD_KEY_INSERT             = 260,
    ENGINE_KEYBOARD_KEY_DELETE             = 261,
    ENGINE_KEYBOARD_KEY_RIGHT              = 262,
    ENGINE_KEYBOARD_KEY_LEFT               = 263,
    ENGINE_KEYBOARD_KEY_DOWN               = 264,
    ENGINE_KEYBOARD_KEY_UP                 = 265,
    ENGINE_KEYBOARD_KEY_PAGE_UP            = 266,
    ENGINE_KEYBOARD_KEY_PAGE_DOWN          = 267,
    ENGINE_KEYBOARD_KEY_HOME               = 268,
    ENGINE_KEYBOARD_KEY_END                = 269,
    ENGINE_KEYBOARD_KEY_CAPS_LOCK          = 280,
    ENGINE_KEYBOARD_KEY_SCROLL_LOCK        = 281,
    ENGINE_KEYBOARD_KEY_NUM_LOCK           = 282,
    ENGINE_KEYBOARD_KEY_PRINT_SCREEN       = 283,
    ENGINE_KEYBOARD_KEY_PAUSE              = 284,
    ENGINE_KEYBOARD_KEY_F1                 = 290,
    ENGINE_KEYBOARD_KEY_F2                 = 291,
    ENGINE_KEYBOARD_KEY_F3                 = 292,
    ENGINE_KEYBOARD_KEY_F4                 = 293,
    ENGINE_KEYBOARD_KEY_F5                 = 294,
    ENGINE_KEYBOARD_KEY_F6                 = 295,
    ENGINE_KEYBOARD_KEY_F7                 = 296,
    ENGINE_KEYBOARD_KEY_F8                 = 297,
    ENGINE_KEYBOARD_KEY_F9                 = 298,
    ENGINE_KEYBOARD_KEY_F10                = 299,
    ENGINE_KEYBOARD_KEY_F11                = 300,
    ENGINE_KEYBOARD_KEY_F12                = 301,
    ENGINE_KEYBOARD_KEY_F13                = 302,
    ENGINE_KEYBOARD_KEY_F14                = 303,
    ENGINE_KEYBOARD_KEY_F15                = 304,
    ENGINE_KEYBOARD_KEY_F16                = 305,
    ENGINE_KEYBOARD_KEY_F17                = 306,
    ENGINE_KEYBOARD_KEY_F18                = 307,
    ENGINE_KEYBOARD_KEY_F19                = 308,
    ENGINE_KEYBOARD_KEY_F20                = 309,
    ENGINE_KEYBOARD_KEY_F21                = 310,
    ENGINE_KEYBOARD_KEY_F22                = 311,
    ENGINE_KEYBOARD_KEY_F23                = 312,
    ENGINE_KEYBOARD_KEY_F24                = 313,
    ENGINE_KEYBOARD_KEY_F25                = 314,
    ENGINE_KEYBOARD_KEY_KP_0               = 320,
    ENGINE_KEYBOARD_KEY_KP_1               = 321,
    ENGINE_KEYBOARD_KEY_KP_2               = 322,
    ENGINE_KEYBOARD_KEY_KP_3               = 323,
    ENGINE_KEYBOARD_KEY_KP_4               = 324,
    ENGINE_KEYBOARD_KEY_KP_5               = 325,
    ENGINE_KEYBOARD_KEY_KP_6               = 326,
    ENGINE_KEYBOARD_KEY_KP_7               = 327,
    ENGINE_KEYBOARD_KEY_KP_8               = 328,
    ENGINE_KEYBOARD_KEY_KP_9               = 329,
    ENGINE_KEYBOARD_KEY_KP_DECIMAL         = 330,
    ENGINE_KEYBOARD_KEY_KP_DIVIDE          = 331,
    ENGINE_KEYBOARD_KEY_KP_MULTIPLY        = 332,
    ENGINE_KEYBOARD_KEY_KP_SUBTRACT        = 333,
    ENGINE_KEYBOARD_KEY_KP_ADD             = 334,
    ENGINE_KEYBOARD_KEY_KP_ENTER           = 335,
    ENGINE_KEYBOARD_KEY_KP_EQUAL           = 336,
    ENGINE_KEYBOARD_KEY_LEFT_SHIFT         = 340,
    ENGINE_KEYBOARD_KEY_LEFT_CONTROL       = 341,
    ENGINE_KEYBOARD_KEY_LEFT_ALT           = 342,
    ENGINE_KEYBOARD_KEY_LEFT_SUPER         = 343,
    ENGINE_KEYBOARD_KEY_RIGHT_SHIFT        = 344,
    ENGINE_KEYBOARD_KEY_RIGHT_CONTROL      = 345,
    ENGINE_KEYBOARD_KEY_RIGHT_ALT          = 346,
    ENGINE_KEYBOARD_KEY_RIGHT_SUPER        = 347,
    ENGINE_KEYBOARD_KEY_MENU               = 348,
} engine_keyboard_keys_t;

typedef enum _engine_texture_color_space_t
{
    ENGINE_TEXTURE_COLOR_SPACE_SRGB = 0,
    ENGINE_TEXTURE_COLOR_SPACE_LINEAR,
} engine_texture_color_space_t;

typedef struct _texture_2d_create_from_data_desc_t
{
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    const void* data;
    engine_texture_color_space_t color_space;
} engine_texture_2d_create_from_memory_desc_t;

typedef enum _engine_result_code_t
{
    ENGINE_RESULT_CODE_OK = 0,
    ENGINE_RESULT_CODE_FAIL = -1
} engine_result_code_t;

typedef struct _engine_vertex_attribute_t
{
    float position[3];
    float uv[2];
} engine_vertex_attribute_t;

ENGINE_API engine_result_code_t engineApplicationCreate(engine_application_t* handle, engine_application_create_desc_t create_desc);
ENGINE_API void engineApplicationDestroy(engine_application_t handle);

ENGINE_API bool engineApplicationIsKeyboardButtonDown(engine_application_t handle, engine_keyboard_keys_t key);
ENGINE_API bool engineApplicationIsKeyboardButtonUp(engine_application_t handle, engine_keyboard_keys_t key);

ENGINE_API engine_mouse_coords_t engineApplicationGetMouseCoords(engine_application_t handle);
ENGINE_API bool engineApplicationIsMouseButtonDown(engine_application_t handle, engine_mouse_button_t);
ENGINE_API bool engineApplicationIsMouseButtonUp(engine_application_t handle, engine_mouse_button_t);

ENGINE_API engine_application_frame_begine_info_t engineApplicationFrameBegine(engine_application_t handle);
ENGINE_API engine_result_code_t                   engineApplicationFrameRunScene(engine_application_t handle, engine_scene_t scene, float delta_time);
ENGINE_API engine_application_frame_end_info_t    engineApplicationFrameEnd(engine_application_t handle);

ENGINE_API engine_result_code_t engineApplicationAddGeometryFromMemory(engine_application_t handle, const engine_vertex_attribute_t* verts, size_t verts_count, uint32_t* inds, size_t inds_count, const char* name, engine_geometry_t* out);

ENGINE_API engine_result_code_t engineApplicationAddTexture2DFromMemory(engine_application_t handle, const engine_texture_2d_create_from_memory_desc_t& info, const char* name, engine_texture2d_t* out);
ENGINE_API engine_result_code_t engineApplicationAddTexture2DFromFile(engine_application_t handle, const char* file_path, engine_texture_color_space_t color_space, const char* name, engine_texture2d_t* out);

ENGINE_API engine_result_code_t engineSceneCreate(engine_scene_t* out);
ENGINE_API void engineSceneDestroy(engine_scene_t scene);

ENGINE_API engine_game_object_t engineSceneCreateGameObject(engine_scene_t scene);
ENGINE_API void                     engineSceneDestroyGameObject(engine_scene_t scene, engine_game_object_t game_object);

ENGINE_API engine_name_component_t* engineSceneAddNameComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_name_component_t* engineSceneGetNameComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void                engineSceneRemoveNameComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool                engineSceneHasNameComponent(engine_scene_t scene, engine_game_object_t game_object);


ENGINE_API engine_tranform_component_t* engineSceneAddTransformComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_tranform_component_t* engineSceneGetTransformComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void                  engineSceneRemoveTransformComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool                  engineSceneHasTransformComponent(engine_scene_t scene, engine_game_object_t game_object);

ENGINE_API engine_mesh_component_t* engineSceneAddMeshComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_mesh_component_t* engineSceneGetMeshComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void              engineSceneRemoveMeshComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool              engineSceneHasMeshComponent(engine_scene_t scene, engine_game_object_t game_object);

ENGINE_API engine_material_component_t* engineSceneAddMaterialComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_material_component_t* engineSceneGetMaterialComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void                             engineSceneRemoveMaterialComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool                             engineSceneHasMaterialComponent(engine_scene_t scene, engine_game_object_t game_object);

ENGINE_API engine_light_component_t* engineSceneAddLightComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_light_component_t* engineSceneGetLightComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void                          engineSceneRemoveLightComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool                          engineSceneHasLightComponent(engine_scene_t scene, engine_game_object_t game_object);

ENGINE_API engine_camera_component_t* engineSceneAddCameraComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_camera_component_t* engineSceneGetCameraComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void                engineSceneRemoveCameraComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool                engineSceneHasCameraComponent(engine_scene_t scene, engine_game_object_t game_object);

#ifdef __cplusplus
}
#endif  // #ifndef __cplusplus