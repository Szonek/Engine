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
    ENGINE_MOUSE_BUTTON_NONE = 0,
    ENGINE_MOUSE_BUTTON_LEFT = 1,
    ENGINE_MOUSE_BUTTON_RIGHT = 2,
    ENGINE_MOUSE_BUTTON_MIDDLE,
} engine_mouse_button_t;

typedef enum _engine_platform_keys_t
{
    ENGINE_KEYBOARD_KEY_UNKNOWN = 0,
    ENGINE_KEYBOARD_KEY_A = 4,
    ENGINE_KEYBOARD_KEY_B = 5,
    ENGINE_KEYBOARD_KEY_C = 6,
    ENGINE_KEYBOARD_KEY_D = 7,
    ENGINE_KEYBOARD_KEY_E = 8,
    ENGINE_KEYBOARD_KEY_F = 9,
    ENGINE_KEYBOARD_KEY_G = 10,
    ENGINE_KEYBOARD_KEY_H = 11,
    ENGINE_KEYBOARD_KEY_I = 12,
    ENGINE_KEYBOARD_KEY_J = 13,
    ENGINE_KEYBOARD_KEY_K = 14,
    ENGINE_KEYBOARD_KEY_L = 15,
    ENGINE_KEYBOARD_KEY_M = 16,
    ENGINE_KEYBOARD_KEY_N = 17,
    ENGINE_KEYBOARD_KEY_O = 18,
    ENGINE_KEYBOARD_KEY_P = 19,
    ENGINE_KEYBOARD_KEY_Q = 20,
    ENGINE_KEYBOARD_KEY_R = 21,
    ENGINE_KEYBOARD_KEY_S = 22,
    ENGINE_KEYBOARD_KEY_T = 23,
    ENGINE_KEYBOARD_KEY_U = 24,
    ENGINE_KEYBOARD_KEY_V = 25,
    ENGINE_KEYBOARD_KEY_W = 26,
    ENGINE_KEYBOARD_KEY_X = 27,
    ENGINE_KEYBOARD_KEY_Y = 28,
    ENGINE_KEYBOARD_KEY_Z = 29,
    ENGINE_KEYBOARD_KEY_1 = 30,
    ENGINE_KEYBOARD_KEY_2 = 31,
    ENGINE_KEYBOARD_KEY_3 = 32,
    ENGINE_KEYBOARD_KEY_4 = 33,
    ENGINE_KEYBOARD_KEY_5 = 34,
    ENGINE_KEYBOARD_KEY_6 = 35,
    ENGINE_KEYBOARD_KEY_7 = 36,
    ENGINE_KEYBOARD_KEY_8 = 37,
    ENGINE_KEYBOARD_KEY_9 = 38,
    ENGINE_KEYBOARD_KEY_0 = 39,
    ENGINE_KEYBOARD_KEY_ENTER = 40,
    ENGINE_KEYBOARD_KEY_ESCAPE = 41,
    ENGINE_KEYBOARD_KEY_BACKSPACE = 42,
    ENGINE_KEYBOARD_KEY_TAB = 43,
    ENGINE_KEYBOARD_KEY_SPACE = 44,
    ENGINE_KEYBOARD_KEY_PAGEDOWN = 78,
    ENGINE_KEYBOARD_KEY_RIGHT = 79,
    ENGINE_KEYBOARD_KEY_LEFT = 80,
    ENGINE_KEYBOARD_KEY_DOWN = 81,
    ENGINE_KEYBOARD_KEY_UP = 82,
    ENGINE_KEYBOARD_KEY_NUMLOCKCLEAR = 83,
    ENGINE_KEYBOARD_KEY_KP_DIVIDE = 84,
    ENGINE_KEYBOARD_KEY_KP_MULTIPLY = 85,
    ENGINE_KEYBOARD_KEY_KP_MINUS = 86,
    ENGINE_KEYBOARD_KEY_KP_PLUS = 87,
    ENGINE_KEYBOARD_KEY_KP_ENTER = 88,
    ENGINE_KEYBOARD_KEY_LCTRL = 224,
    ENGINE_KEYBOARD_KEY_LSHIFT = 225,
    ENGINE_KEYBOARD_KEY_LALT = 226, /**< alt, option */
    ENGINE_KEYBOARD_KEY_LGUI = 227, /**< windows, command (apple), meta */
    ENGINE_KEYBOARD_KEY_RCTRL = 228,
    ENGINE_KEYBOARD_KEY_RSHIFT = 229,
    ENGINE_KEYBOARD_KEY_RALT = 230, /**< alt gr, option */
} engine_keyboard_keys_t;

typedef enum _engine_result_code_t
{
    ENGINE_RESULT_CODE_OK = 0,
    ENGINE_RESULT_CODE_FAIL = -1
} engine_result_code_t;

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

//ENGINE_API engine_geometry_t   engineApplicationGetDefaultGeometry(engine_application_handle_t handle, engine_default_geometry_type_t shape_type);
//ENGINE_API engine_program_t    engineApplicationGetDefaultProgram(engine_application_handle_t handle, engine_default_program_t program_type);
//ENGINE_API engine_texture_2d_t engineApplicationGetDefaultTexture2D(engine_application_handle_t handle);

//ENGINE_API engine_texture_2d_t engineApplicationAddTexture2DFromData(engine_application_handle_t handle, const engine_texture_2d_create_from_data_desc_t& info, const char* name);
//ENGINE_API engine_texture_2d_t engineApplicationAddTexture2DFromFile(engine_application_handle_t handle, const char* file_path, engine_texture_color_space_t color_space, const char* name);


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