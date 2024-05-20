#pragma once

#include "components/camera_component.h"
#include "components/light_component.h"
#include "components/transform_component.h"
#include "components/name_component.h"
#include "components/mesh_component.h"
#include "components/material_component.h"
#include "components/rigid_body_component.h"
#include "components/collider_component.h"
#include "components/parent_component.h"


#ifdef _WIN32
#ifdef engine_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
#else
#define ENGINE_API
#endif

#define ENGINE_BIT_MASK(id) 1 << id
#define ENGINE_BIT_MASK_MAX 0x7FFFFFFF

#define ENGINE_APPLICATION_NAME_MAX_LENGTH 256

#ifdef __cplusplus
extern "C"
{
#endif  // #ifndef __cplusplus

#include <stdint.h>
#include <limits.h>

typedef uint32_t engine_game_object_t;
#define ENGINE_INVALID_GAME_OBJECT_ID 0
#define ENGINE_INVALID_OBJECT_HANDLE (UINT_MAX)
typedef struct _engine_application_t* engine_application_t;
typedef struct _engine_scene_t* engine_scene_t;
typedef struct _engine_component_view_t* engine_component_view_t;
typedef struct _engine_component_iterator_t* engine_component_iterator_t;
typedef struct _engine_ui_document_t* engine_ui_document_t;
typedef struct _engine_ui_data_handle_t* engine_ui_data_handle_t;
typedef struct _engine_ui_element_t* engine_ui_element_t;
typedef uint32_t engine_material_t;
typedef uint32_t engine_texture2d_t;
typedef uint32_t engine_geometry_t;

typedef struct _engine_coords_2d_t
{
    float x;
    float y;
} engine_coords_2d_t;

typedef struct _engine_ray_t
{
    float origin[3];
    float direction[3];
} engine_ray_t;

typedef struct _engine_ray_hit_info_t
{
    engine_game_object_t go;
    float position[3];
    float normal[3];
} engine_ray_hit_info_t;

typedef enum _engine_ui_document_data_binding_data_type_t
{
    ENGINE_DATA_TYPE_UNKNOWN = 0,
    ENGINE_DATA_TYPE_BOOL = 1,
    ENGINE_DATA_TYPE_UINT32
} engine_ui_document_data_binding_data_type_t;

typedef struct _engine_ui_document_data_binding_t
{
    const char* name;
    engine_ui_document_data_binding_data_type_t type;
    union
    {
        bool* data_bool;
        uint32_t* data_uint32_t;
    };
} engine_ui_document_data_binding_t;


typedef enum _engine_ui_event_type_t
{
    ENGINE_UI_EVENT_TYPE_UNKNOWN = 0,            // unknown/invalid/error
    ENGINE_UI_EVENT_TYPE_POINTER_CLICK = 1,      // single click (pressed and released on the same component)
    ENGINE_UI_EVENT_TYPE_POINTER_DOWN,           // single click  (moment, when pointer is pressed on the compontnet)
    ENGINE_UI_EVENT_TYPE_POINTER_UP,             // single click  (moment, when pointer is released on the compontnet)
    ENGINE_UI_EVENT_TYPE_POINTER_MOVE,           // pointer move over the component
} engine_ui_event_type_t;




typedef struct _engine_ui_event_t
{
    engine_ui_event_type_t type;
    engine_coords_2d_t normalized_screen_position;
} engine_ui_event_t;


typedef struct _engine_application_create_desc_t
{
    const char* name;
    const char* asset_store_path;
    uint32_t width;
    uint32_t height;
    bool fullscreen;
    bool enable_editor;
} engine_application_create_desc_t;

typedef struct _engine_scene_create_desc_t
{
} engine_scene_create_desc_t;

typedef enum _engine_begin_frame_event_flags_t
{
    ENGINE_EVENT_NONE = 0x0,
    ENGINE_EVENT_QUIT = 0x2,
    ENGINE_EVENT_WINDOW_MOVED = 0x4,
    ENGINE_EVENT_WINDOW_RESIZED = 0x8,
} engine_begin_frame_event_flags_t;
typedef uint32_t engine_begin_frame_events_mask_t;

typedef struct _engine_application_frame_begine_info_t
{
    float delta_time;
    engine_begin_frame_events_mask_t events;
} engine_application_frame_begine_info_t;

typedef struct _engine_application_frame_end_info_t
{
    bool success;
} engine_application_frame_end_info_t;

typedef enum _engine_data_layout_t
{
    ENGINE_DATA_LAYOUT_RGBA_U8 = 0,
    ENGINE_DATA_LAYOUT_RGB_U8 = 1,
    ENGINE_DATA_LAYOUT_R_U8 = 2,
    // ..
    // ..
    // ..
    ENGINE_DATA_LAYOUT_RGBA_FP32,
    ENGINE_DATA_LAYOUT_R_FP32,
    ENGINE_DATA_LAYOUT_COUNT
} engine_data_layout_t;

typedef enum _engine_finger_event_t
{
    ENGINE_FINGER_UNKNOWN = 0x0,
    ENGINE_FINGER_DOWN = 0x1,
    ENGINE_FINGER_MOTION = 0x2,
    ENGINE_FINGER_UP = 0x4
} engine_finger_event_t;

typedef struct _engine_finger_info_t
{
    uint32_t event_type_flags;
    float x;  // pos normalized (0, 1)
    float y;  // pos normalized (0, 1)
    float dx; // normalized (-1, 1) delta of movement (if motion detected)
    float dy; // normalized (-1, 1) delta of movement (if motion detected)
} engine_finger_info_t;

#define ENGINE_FINGERS_INFOS_LIST_COUNT 10
typedef struct _engine_fingers_infos_list_t
{
    engine_finger_info_t infos[ENGINE_FINGERS_INFOS_LIST_COUNT];
} engine_fingers_infos_list_t;

typedef enum _engine_mouse_button_t
{
    ENGINE_MOUSE_BUTTON_UNKNOWN  = 0,
    ENGINE_MOUSE_BUTTON_LEFT     = 1,
    ENGINE_MOUSE_BUTTON_MIDDLE   = 2,
    ENGINE_MOUSE_BUTTON_RIGHT    = 3,
    ENGINE_MOUSE_BUTTON_X1       = 4,
    ENGINE_MOUSE_BUTTON_X2       = 5,
    ENGINE_MOUSE_BUTTON_COUNT
} engine_mouse_button_t;

typedef enum _engine_keyboard_keys_t
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

typedef enum _engine_texture_color_space_t
{
    ENGINE_TEXTURE_COLOR_SPACE_SRGB = 0,
    ENGINE_TEXTURE_COLOR_SPACE_LINEAR,
} engine_texture_color_space_t;

typedef struct _texture_2d_create_desc_t
{
    uint32_t width;
    uint32_t height;
    engine_data_layout_t data_layout;
    const void* data;
} engine_texture_2d_create_desc_t;

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

typedef enum _engine_vertex_attribute_type_t
{
    ENGINE_VERTEX_ATTRIBUTE_TYPE_POSITION = 0,
    ENGINE_VERTEX_ATTRIBUTE_TYPE_UV_0,
    ENGINE_VERTEX_ATTRIBUTE_TYPE_NORMALS,
    ENGINE_VERTEX_ATTRIBUTE_TYPE_JOINTS_0,
    ENGINE_VERTEX_ATTRIBUTE_TYPE_WEIGHTS_0,

    //
    // 
 
    ENGINE_VERTEX_ATTRIBUTE_TYPE_COUNT,
} engine_vertex_attribute_type_t;

typedef enum _engine_vertex_attribute_data_type_t
{
    ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_UNKNOWN = 0,
    ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_FLOAT32 = 1,

    ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_UINT32,
    ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_INT32,

    ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_UINT16,
    ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_INT16,

    ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_UINT8,
    ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_INT8,

} engine_vertex_attribute_data_type_t;

typedef struct _engine_vertex_attribute_desc_t
{
    uint32_t elements_count;  // set to 0 to disable given attribute
    engine_vertex_attribute_data_type_t elements_data_type;
    engine_vertex_attribute_type_t type;

    float range_min[4];
    float range_max[4];
} engine_vertex_attribute_desc_t;

typedef struct _engine_vertex_attributes_layout_t
{
    engine_vertex_attribute_desc_t attributes[ENGINE_VERTEX_ATTRIBUTE_TYPE_COUNT];
} engine_vertex_attributes_layout_t;

typedef struct _engine_collision_contact_t
{
    float point_object_a[3];
    float point_object_b[3];
    int32_t lifetime;
} engine_collision_contact_point_t;

typedef struct _engine_collision_info_t
{
    engine_game_object_t object_a;
    engine_game_object_t object_b;

    size_t contact_points_count;
    const engine_collision_contact_point_t* contact_points;

} engine_collision_info_t;

typedef enum _engine_model_specification_t
{
    ENGINE_MODEL_SPECIFICATION_GLTF_2
} engine_model_specification_t;

typedef struct _engine_geometry_create_desc_t
{
    const void* verts_data;
    size_t verts_data_size;
    int32_t verts_count;
    engine_vertex_attributes_layout_t verts_layout;
    const uint32_t* inds;
    size_t inds_count;
} engine_geometry_create_desc_t;

typedef struct _engine_animation_channel_data_t
{
    const float* timestamps;
    uint32_t timestamps_count;

    const float* data;
    size_t data_count;
} engine_animation_channel_data_t;

typedef struct _engine_animation_channel_create_desc_t
{
    uint32_t model_node_index;
    engine_animation_channel_data_t channel_translation;  // data is {x, y, z}
    engine_animation_channel_data_t channel_rotation;  // data is quatenion: {x, y, z, w}
    engine_animation_channel_data_t channel_scale;  // data is {x, y, z}
} engine_animation_channel_create_desc_t;

typedef struct _engine_animation_clip_create_desc_t
{
    const char* name;
    engine_animation_channel_create_desc_t* channels;
    uint32_t channels_count;
} engine_animation_clip_create_desc_t;

typedef struct _engine_material_create_desc_t
{
    float diffuse_color[3];
    uint32_t shininess;
    engine_texture2d_t diffuse_texture;
    engine_texture2d_t specular_texture;
} engine_material_create_desc_t;

typedef struct _engine_bones_create_desc_t
{
    uint32_t model_node_index;
    float inverse_bind_mat[16];
} engine_bone_create_desc_t;

typedef struct _engine_skin_reate_desc_t
{
    const char* name;
    engine_bone_create_desc_t* bones_array;
    uint32_t bones_count;
} engine_skin_create_desc_t;

typedef struct _engine_model_material_desc_t
{
    const char* name;
    float diffuse_color[3];
    uint32_t diffuse_texture_index;  // -1 if not used
} engine_model_material_desc_t;

typedef struct _engine_model_node_desc_t
{
    const char* name;
    struct _engine_model_node_desc_t* parent; // nullptr if no parent
    uint32_t geometry_index;  // -1 if not used
    uint32_t skin_index;      // -1 if not used
    uint32_t material_index;  // -1 if not used
    float translate[3];
    float scale[3];
    float rotation_quaternion[4];
} engine_model_node_desc_t;

typedef struct _engine_model_desc_t
{
    const void* internal_handle;

    engine_model_node_desc_t* nodes_array;
    uint32_t nodes_count;

    engine_geometry_create_desc_t* geometries_array;
    uint32_t geometries_count;

    engine_model_material_desc_t* materials_array;
    uint32_t materials_count;

    engine_texture_2d_create_desc_t* textures_array;
    uint32_t textures_count;

    engine_animation_clip_create_desc_t* animations_array;
    uint32_t animations_counts;

    engine_skin_create_desc_t* skins_array;
    uint32_t skins_counts;
} engine_model_desc_t;

/**
 * @struct engine_geometry_attribute_limit_t
 * @brief A structure representing the limits of a geometry attribute in the engine.
 *
 * This structure is used to define the limits or boundaries of a specific attribute of a geometry object in the engine.
 * This could be used for various purposes such as optimization, error checking, or to define certain behaviors in the engine.
 *
 * @var float min
 * The minimum value of the attribute. This represents the lower limit or boundary for this attribute.
 *
 * @var float max
 * The maximum value of the attribute. This represents the upper limit or boundary for this attribute.
 */
typedef struct _engine_geometry_attribute_limit_t
{
    size_t elements_count;
    float min[4];
    float max[4];
} engine_geometry_attribute_limit_t;


// cross platform log
ENGINE_API void engineLog(const char* str);

// app
ENGINE_API engine_result_code_t engineApplicationCreate(engine_application_t* handle, engine_application_create_desc_t create_desc);
ENGINE_API void engineApplicationDestroy(engine_application_t handle);

// scene
ENGINE_API engine_result_code_t engineApplicationSceneCreate(engine_application_t handle, engine_scene_create_desc_t desc, engine_scene_t* out);
ENGINE_API void engineApplicationSceneDestroy(engine_application_t handle, engine_scene_t scene);

// game objects in scene
ENGINE_API engine_game_object_t engineSceneCreateGameObject(engine_scene_t scene);
ENGINE_API void                 engineSceneDestroyGameObject(engine_scene_t scene, engine_game_object_t game_object);

// user input hangling
ENGINE_API bool engineApplicationIsKeyboardButtonDown(engine_application_t handle, engine_keyboard_keys_t key);
ENGINE_API bool engineApplicationIsKeyboardButtonUp(engine_application_t handle, engine_keyboard_keys_t key);

ENGINE_API engine_coords_2d_t engineApplicationGetMouseCoords(engine_application_t handle);
ENGINE_API bool engineApplicationIsMouseButtonDown(engine_application_t handle, engine_mouse_button_t);
ENGINE_API bool engineApplicationIsMouseButtonUp(engine_application_t handle, engine_mouse_button_t);

ENGINE_API bool engineApplicationGetFingerInfo(engine_application_t handle, engine_fingers_infos_list_t* infos_list);

//frame handling
ENGINE_API engine_application_frame_begine_info_t engineApplicationFrameBegine(engine_application_t handle);
ENGINE_API engine_result_code_t                   engineApplicationFrameSceneUpdate(engine_application_t handle, engine_scene_t scene, float delta_time);
ENGINE_API engine_application_frame_end_info_t    engineApplicationFrameEnd(engine_application_t handle);

// fonts
ENGINE_API engine_result_code_t engineApplicationAddFontFromFile(engine_application_t handle, const char* file_name, const char* handle_name);

// model loading
ENGINE_API engine_result_code_t engineApplicationAllocateModelDescAndLoadDataFromFile(engine_application_t handle, engine_model_specification_t spec, const char* file_name, const char* base_dir, engine_model_desc_t* out);
ENGINE_API void engineApplicationReleaseModelDesc(engine_application_t handle, engine_model_desc_t* model_info);

// geometry
ENGINE_API engine_result_code_t engineApplicationAddGeometryFromDesc(engine_application_t handle, const engine_geometry_create_desc_t* desc, const char* name, engine_geometry_t* out);
ENGINE_API engine_geometry_t engineApplicationGetGeometryByName(engine_application_t handle, const char* name);
ENGINE_API engine_geometry_attribute_limit_t engineApplicationGeometryGetAttributeLimits(engine_application_t handle, engine_geometry_t geometry, engine_vertex_attribute_type_t type);

// material
ENGINE_API engine_material_create_desc_t engineApplicationInitMaterialDesc(engine_application_t handle);
ENGINE_API engine_result_code_t engineApplicationAddMaterialFromDesc(engine_application_t handle, const engine_material_create_desc_t* desc, const char* name, engine_material_t* out);
ENGINE_API engine_material_t    engineApplicationGetMaterialByName(engine_application_t handle, const char* name);

// textures 
ENGINE_API engine_result_code_t engineApplicationAddTexture2DFromDesc(engine_application_t handle, const engine_texture_2d_create_desc_t* info, const char* name, engine_texture2d_t* out);
ENGINE_API engine_result_code_t engineApplicationAddTexture2DFromFile(engine_application_t handle, const char* file_path, engine_texture_color_space_t color_space, const char* name, engine_texture2d_t* out);
ENGINE_API engine_texture2d_t   engineApplicationGetTextured2DByName(engine_application_t handle, const char* name);

// physics 
ENGINE_API void engineScenePhysicsSetGravityVector(engine_scene_t scene, const float gravity[3]);
ENGINE_API void engineScenePhysicsGetCollisions(engine_scene_t scene, size_t* num_collision, const engine_collision_info_t** collisions);
ENGINE_API engine_ray_hit_info_t engineScenePhysicsRayCast(engine_scene_t scene, const engine_game_object_t* ignore_list, size_t ignore_list_count, const engine_ray_t* ray, float max_distance);

// ui
// create data handel first, before loading document!
ENGINE_API engine_result_code_t engineApplicationCreateUiDocumentDataHandle(engine_application_t app, const char* name, const engine_ui_document_data_binding_t* bindings, size_t bindings_count, engine_ui_data_handle_t* out);
ENGINE_API void engineUiDataHandleDestroy(engine_ui_data_handle_t handle);
ENGINE_API void engineUiDataHandleDirtyAllVariables(engine_ui_data_handle_t handle);
ENGINE_API void engineUiDataHandleDirtyVariable(engine_ui_data_handle_t handle, const char* name);

// if document uses data model than creata data model first with function: engineApplicationCreateUiDataHandle(...)
ENGINE_API engine_result_code_t engineApplicationCreateUiDocumentFromFile(engine_application_t app, const char* file_path, engine_ui_document_t* out);
ENGINE_API void engineApplicationUiDocumentDestroy(engine_ui_document_t doc);
ENGINE_API void engineUiDocumentShow(engine_ui_document_t ui_doc);
ENGINE_API void engineUiDocumentHide(engine_ui_document_t ui_doc);
// interanlly it caches elements, so it's safe to call it multiple times to get the same object
// however for best performance it's recommeneded to reuse elements and minimize usage of this function
ENGINE_API engine_result_code_t engineUiDocumentGetElementById(engine_ui_document_t document, const char* id, engine_ui_element_t* out);
ENGINE_API engine_result_code_t engineUiElementAddEventCallback(engine_ui_element_t element, engine_ui_event_type_t event_type, void* user_data, void (*callback)(const engine_ui_event_t*, void*));
ENGINE_API engine_result_code_t engineUiElementSetProperty(engine_ui_element_t element, const char* property, const char* value);
ENGINE_API void engineUiElementRemoveProperty(engine_ui_element_t element, const char* property);


// ECS 
ENGINE_API engine_result_code_t engineCreateComponentView(engine_component_view_t* out);
ENGINE_API void engineDestroyComponentView(engine_component_view_t view);
ENGINE_API engine_result_code_t engineComponentViewCreateBeginComponentIterator(engine_component_view_t view, engine_component_iterator_t* out);
ENGINE_API engine_result_code_t engineComponentViewCreateEndComponentIterator(engine_component_view_t view, engine_component_iterator_t* out);
ENGINE_API bool engineComponentIteratorCheckEqual(engine_component_iterator_t lhs, engine_component_iterator_t rhs);
ENGINE_API void engineComponentIteratorNext(engine_component_iterator_t iterator);
ENGINE_API engine_game_object_t engineComponentIteratorGetGameObject(engine_component_iterator_t iterator);
ENGINE_API void engineDeleteComponentIterator(engine_component_iterator_t iterator);

// name component
ENGINE_API engine_name_component_t engineSceneAddNameComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_name_component_t engineSceneGetNameComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void engineSceneUpdateNameComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_name_component_t* comp);
ENGINE_API void engineSceneRemoveNameComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool engineSceneHasNameComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void engineSceneComponentViewAttachNameComponent(engine_scene_t scene, engine_component_view_t view);

// transform component
ENGINE_API engine_tranform_component_t engineSceneAddTransformComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_tranform_component_t engineSceneGetTransformComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void engineSceneUpdateTransformComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_tranform_component_t* comp);
ENGINE_API void engineSceneRemoveTransformComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool engineSceneHasTransformComponent(engine_scene_t scene, engine_game_object_t game_object);

// light component
ENGINE_API engine_light_component_t engineSceneAddLightComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_light_component_t engineSceneGetLightComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void engineSceneUpdateLightComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_light_component_t* comp);
ENGINE_API void engineSceneRemoveLightComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool engineSceneHasLightComponent(engine_scene_t scene, engine_game_object_t game_object);

// mesh component
ENGINE_API engine_mesh_component_t engineSceneAddMeshComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_mesh_component_t engineSceneGetMeshComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void engineSceneUpdateMeshComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_mesh_component_t* comp);
ENGINE_API void engineSceneRemoveMeshComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool engineSceneHasMeshComponent(engine_scene_t scene, engine_game_object_t game_object);

// skin component
ENGINE_API engine_skin_component_t engineSceneAddSkinComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_skin_component_t engineSceneGetSkinComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void engineSceneUpdateSkinComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_skin_component_t* comp);
ENGINE_API void engineSceneRemoveSkinComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool engineSceneHasSkinComponent(engine_scene_t scene, engine_game_object_t game_object);

// bones component
ENGINE_API engine_bone_component_t engineSceneAddBoneComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_bone_component_t engineSceneGetBoneComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void engineSceneUpdateBoneComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_bone_component_t* comp);
ENGINE_API void engineSceneRemoveBoneComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool engineSceneHasBoneComponent(engine_scene_t scene, engine_game_object_t game_object);

// material component
ENGINE_API engine_material_component_t engineSceneAddMaterialComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_material_component_t engineSceneGetMaterialComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void engineSceneUpdateMaterialComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_material_component_t* comp);
ENGINE_API void engineSceneRemoveMaterialComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool engineSceneHasMaterialComponent(engine_scene_t scene, engine_game_object_t game_object);

// light component
ENGINE_API engine_light_component_t engineSceneAddLightComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_light_component_t engineSceneGetLightComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void engineSceneUpdateLightComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_light_component_t* comp);
ENGINE_API void engineSceneRemoveLightComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool engineSceneHasLightComponent(engine_scene_t scene, engine_game_object_t game_object);

// camera component
ENGINE_API engine_camera_component_t engineSceneAddCameraComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_camera_component_t engineSceneGetCameraComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void engineSceneUpdateCameraComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_camera_component_t* comp);
ENGINE_API void engineSceneRemoveCameraComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool engineSceneHasCameraComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void engineSceneComponentViewAttachCameraComponent(engine_scene_t scene, engine_component_view_t view);

// rigid body component
ENGINE_API engine_rigid_body_component_t engineSceneAddRigidBodyComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_rigid_body_component_t engineSceneGetRigidBodyComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void engineSceneUpdateRigidBodyComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_rigid_body_component_t* comp);
ENGINE_API void engineSceneRemoveRigidBodyComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool engineSceneHasRigidBodyComponent(engine_scene_t scene, engine_game_object_t game_object);

// collider component
ENGINE_API engine_collider_component_t engineSceneAddColliderComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_collider_component_t engineSceneGetColliderComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void engineSceneUpdateColliderComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_collider_component_t* comp);
ENGINE_API void engineSceneRemoveColliderComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool engineSceneHasColliderComponent(engine_scene_t scene, engine_game_object_t game_object);

// parent component
ENGINE_API engine_parent_component_t engineSceneAddParentComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API engine_parent_component_t engineSceneGetParentComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API void engineSceneUpdateParentComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_parent_component_t* comp);
ENGINE_API void engineSceneRemoveParentComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool engineSceneHasParentComponent(engine_scene_t scene, engine_game_object_t game_object);

// children component - only getters - it's managed internally by engine and user can't update it's state 
ENGINE_API engine_children_component_t engineSceneGetChildrenComponent(engine_scene_t scene, engine_game_object_t game_object);
ENGINE_API bool engineSceneHasChildrenComponent(engine_scene_t scene, engine_game_object_t game_object);
#ifdef __cplusplus
}
#endif  // #ifndef __cplusplus