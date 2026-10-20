#pragma once
#include "engine_defs.h"

#include "components/camera_component.h"
#include "components/light_component.h"
#include "components/transform_component.h"
#include "components/name_component.h"
#include "components/folder_component.h"
#include "components/mesh_component.h"
#include "components/material_component.h"
#include "components/rigid_body_component.h"
#include "components/collider_component.h"
#include "components/parent_component.h"
#include "components/sprite_component.h"
#include "components/skin_component.h"
#include "components/animation_controller_component.h"

#include "engine_string.h"
#include "engine_vector.h"


#ifdef __cplusplus
extern "C"
{
#endif  // #ifndef __cplusplus

#include <stdint.h>
#include <limits.h>

typedef uint32_t engine_game_object_t;
#define ENGINE_INVALID_GAME_OBJECT_ID 0
// Asset handles (textures, geometries, prefabs, ...) are not reused: a handle to an asset which was destroyed stays
// invalid (lookups with it find nothing) when other assets are created afterwards, until its slot has been reused about
// four million times.
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
typedef uint32_t engine_prefab_t;
typedef uint32_t engine_skin_t;
typedef uint32_t engine_animation_set_t;
typedef uint32_t engine_shader_t;
typedef uint32_t engine_animation_event_id_t;

typedef enum _engine_result_code_t
{
    ENGINE_RESULT_CODE_OK = 0,
    ENGINE_RESULT_CODE_FAIL = -1,
    ENGINE_RESULT_CODE_INVALID_ARG = -2
} engine_result_code_t;

typedef struct _engine_fvec2_t
{
    float x;
    float y;
} engine_fvec2_t;

typedef struct _engine_fvec3_t
{
    float x;
    float y;
    float z;
} engine_fvec3_t;

typedef struct _engine_fvec4_t
{
    float x;
    float y;
    float z;
    float w;
} engine_fvec4_t;

typedef struct _engine_application_create_desc_t
{
    const char* name;
    const char* asset_store_path;
    uint32_t width;
    uint32_t height;
    bool fullscreen;
    bool enable_editor;
    // create the window hidden (e.g. automated tests); frames still render but are never presented
    bool hidden_window;
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
    ENGINE_MOUSE_BUTTON_UNKNOWN = 0,
    ENGINE_MOUSE_BUTTON_LEFT = 1,
    ENGINE_MOUSE_BUTTON_MIDDLE = 2,
    ENGINE_MOUSE_BUTTON_RIGHT = 3,
    ENGINE_MOUSE_BUTTON_X1 = 4,
    ENGINE_MOUSE_BUTTON_X2 = 5,
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

typedef struct _engine_ray_t
{
    engine_fvec3_t origin;
    engine_fvec3_t direction;
} engine_ray_t;

typedef struct _engine_ray_hit_info_t
{
    engine_game_object_t go;
    engine_fvec3_t position;
    engine_fvec3_t normal;
} engine_ray_hit_info_t;

typedef enum _engine_ui_event_type_t
{
    ENGINE_UI_EVENT_TYPE_UNKNOWN = 0,            // unknown/invalid/error
    ENGINE_UI_EVENT_TYPE_POINTER_CLICK = 1,      // single click (pressed and released on the same component)
    ENGINE_UI_EVENT_TYPE_POINTER_DOWN,           // single click  (moment, when pointer is pressed on the compontnet)
    ENGINE_UI_EVENT_TYPE_POINTER_UP,             // single click  (moment, when pointer is released on the compontnet)
    ENGINE_UI_EVENT_TYPE_POINTER_MOVE,           // pointer moves over the component
    ENGINE_UI_EVENT_TYPE_POINTER_OVER,           // pointer moves into the component
    ENGINE_UI_EVENT_TYPE_POINTER_OUT,            // pointer moves out from the component
} engine_ui_event_type_t;

typedef struct _engine_ui_event_t
{
    engine_ui_event_type_t type;
    engine_fvec2_t normalized_screen_position;
} engine_ui_event_t;

typedef enum _engine_ui_data_type_t
{
    ENGINE_UI_DATA_TYPE_UNKNOWN = 0,
    
    ENGINE_UI_DATA_TYPE_BOOL = 1,

    ENGINE_UI_DATA_TYPE_UINT32,
    ENGINE_UI_DATA_TYPE_VECTOR_UINT32,

    ENGINE_UI_DATA_TYPE_STRING,
    ENGINE_UI_DATA_TYPE_VECTOR_STRING,

    ENGINE_UI_DATA_TYPE_EVENT_CALLBACK,
} engine_ui_data_type_t;

typedef struct _engine_ui_data_variant_t
{
    engine_ui_data_type_t type;
    union
    {
        bool b;
        uint32_t u32;
    } arg;
} engine_ui_data_variant_t;

typedef struct _engine_ui_document_data_binding_event_callback_t
{
    void (*fn_ptr)(engine_ui_data_handle_t data_handle, const engine_ui_event_t* event, const engine_vector_engine_ui_data_variant_t args, void* user_data);
    void* user_data;
} engine_ui_document_data_binding_event_callback_t;

typedef struct _engine_ui_document_data_binding_t
{
    const char* name;
    engine_ui_data_type_t type;
    union
    {
        bool* data_bool;

        uint32_t* data_uint32_t;
        engine_vector_uint32_t data_vector_uint32_t;

        engine_string_t* data_string;
        engine_vector_engine_string_t data_vector_string;

        engine_ui_document_data_binding_event_callback_t data_callback;
    };
} engine_ui_document_data_binding_t;

typedef struct _engine_vertex_attribute_t
{
    engine_fvec3_t position;
    engine_fvec2_t uv;
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

typedef struct _engine_collision_contact_point_desc_t engine_collision_contact_point_desc_t;
ENGINE_API engine_fvec3_t engineCollisionContactPointDescGetPointOnObjectA(const engine_collision_contact_point_desc_t* contact);
ENGINE_API engine_fvec3_t engineCollisionContactPointDescGetPointOnObjectB(const engine_collision_contact_point_desc_t* contact);
ENGINE_API int32_t engineCollisionContactPointDescGetLifetime(const engine_collision_contact_point_desc_t* contact);

typedef struct _engine_collision_desc_t engine_collision_desc_t;
ENGINE_API engine_game_object_t engineCollisionDescGetObjectA(const engine_collision_desc_t* desc);
ENGINE_API engine_game_object_t engineCollisionDescGetObjectB(const engine_collision_desc_t* desc);
ENGINE_API size_t engineCollisionDescGetContactPointsCount(const engine_collision_desc_t* desc);
ENGINE_API const engine_collision_contact_point_desc_t* engineCollisionDescGetContactPoint(const engine_collision_desc_t* desc, size_t idx);

typedef struct _engine_uniform_buffer_create_desc_t
{
    uint32_t size;
} engine_uniform_buffer_create_desc_t;

// can provide multiple files per shader to i.e.: support include files
typedef struct _engine_shader_create_desc_t
{
    const char* const* vertex_shader_filenames;  // null terminated array of string
    const char* const* fragment_shader_filenames; // null terminated array of string
} engine_shader_create_desc_t;

typedef enum _engine_model_specification_t
{
    ENGINE_MODEL_SPECIFICATION_GLTF_2
} engine_model_specification_t;

// Prefab: an asset built from a model file, named by the file ("Rogue.glb"). Loading it also creates the model's
// geometries, textures, skin and animation set as assets named "<file>#<name>" (e.g. "Rogue.glb#Rig", the skin and
// "Rogue.glb#animations", the animation set); they live as long as the prefab. An image in its own file is shared with
// the other models (and textures of the game) using it and lives as long as the last of them.
// Prefabs are reference counted: loading gives the caller a reference to the prefab (loading a model which is loaded
// already returns the same prefab), every instance holds one until it is destroyed, and the prefab is destroyed with
// its assets when the last reference is released. Instantiating it creates the model's game objects in the active
// scene. Components store asset handles without holding references: a game object using a model's asset (e.g. its
// geometry) needs someone to hold the prefab, like the game or an instance.
ENGINE_API engine_result_code_t enginePrefabLoad(engine_model_specification_t spec, const char* file_name, const char* base_dir, engine_prefab_t* out);
ENGINE_API engine_prefab_t enginePrefabGetByName(const char* name);
// Returns the root game object of the new instance, or ENGINE_INVALID_GAME_OBJECT_ID. The root is the parent of all the
// other game objects of the instance.
ENGINE_API engine_game_object_t enginePrefabInstantiate(engine_prefab_t prefab);
// The prefab the game object is the root of an instance of, or ENGINE_INVALID_OBJECT_HANDLE.
ENGINE_API engine_prefab_t enginePrefabInstanceGetPrefab(engine_game_object_t game_object);
ENGINE_API void enginePrefabRelease(engine_prefab_t prefab);

// Skin: an asset shared by every instance of a skinned model (the skeleton and its binding to the mesh), created with
// the model's prefab and named "<model file>#<skin name>". Each instance's pose belongs to the game object with the
// skin component.
ENGINE_API engine_skin_t engineSkinGetByName(const char* name);

// Animation set: an asset with a model's animation clips compiled for its skin, created with the model's prefab, named
// "<model file>#animations" and shared by every controller of that skin.
ENGINE_API engine_animation_set_t engineAnimationSetGetByName(const char* name);

// Animation controller: the runtime part of a game object's animation controller component, created and owned by the
// engine when the component has an animation set and the game object has a skin. The functions below act on it through
// the game object (like enginePhysicsAddForce); they fail (false / an error code) when it has none.
ENGINE_API bool engineAnimationControllerSetPaused(engine_game_object_t game_object, bool paused);
ENGINE_API engine_result_code_t engineAnimationControllerAnimationGetDuration(engine_game_object_t game_object, const char* name, float* out);
ENGINE_API bool engineAnimationControllerIsAnimationPlaying(engine_game_object_t game_object, const char* name);
ENGINE_API bool engineAnimationControllerIsAnimationPlayingOnLayer(engine_game_object_t game_object, const char* name, size_t layer_id);
ENGINE_API bool engineAnimationControllerAnimationPlay(engine_game_object_t game_object, const char* name, size_t layer_id);
// Start fading in from current animation to 'new_animation' over 'duration' seconds
ENGINE_API bool engineAnimationControllerAnimationCrossFade(engine_game_object_t game_object, const char* new_animation_name, size_t layer_id, float duration);
// layers: set up in the animation controller component; these change them while playing
ENGINE_API bool engineAnimationControllerLayerSetWeight(engine_game_object_t game_object, size_t id, float new_weight);
// Multiplies the playback speed of animations on this layer. Must be finite and greater than zero.
ENGINE_API bool engineAnimationControllerLayerSetPlaybackRate(engine_game_object_t game_object, size_t id, float playback_rate);
// Restarts animations on this layer when they reach their end; disabled by default.
ENGINE_API bool engineAnimationControllerLayerSetLooping(engine_game_object_t game_object, size_t id, bool looping);
// events
typedef struct _engine_animation_event_info_t {
    float current_time;
} engine_animation_event_info_t;

typedef struct _engine_animation_event_t {
    float trigger_time;    // Time in seconds when the event should fire
    void (*fn_ptr)(const engine_animation_event_info_t* info, void *user_data);      // Function to call
    void* user_data;       // Optional user data
} engine_animation_event_t;

ENGINE_API engine_result_code_t engineAnimationControllerAnimationAddEvent(engine_game_object_t game_object, const char* anim_name, engine_animation_event_t event, engine_animation_event_id_t* out_id);
ENGINE_API engine_result_code_t engineAnimationControllerAnimationRemoveEvent(engine_game_object_t game_object, const char* anim_name, engine_animation_event_id_t id);
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

/**
 * @enum _engine_force_type_t
 * @brief Specifies the type of force applied to a physics object in the engine.
 *
 * This enumeration is used to define how a force is applied to a game object in the physics simulation.
 *
 *  * @var ENGINE_FORCE_TYPE_FORCE
 *   Continous force applied to the object.
 * 
 * @var ENGINE_FORCE_TYPE_IMPLUSE
 *   Instantaneous force (impulse) applied to the object, causing an immediate change in velocity.
 */
typedef enum _engine_force_type_t
{
    ENGINE_FORCE_TYPE_FORCE = 0, // continous force applied to the object
    ENGINE_FORCE_TYPE_IMPLUSE = 1, // instant force applied to the object
} engine_force_type_t;

// utilities
ENGINE_API void engineLog(const char* str);

typedef struct _engine_profiler_ctx_t engine_profiler_ctx_t;
ENGINE_API engine_profiler_ctx_t* engineProfileZoneStart(const char* name, bool capture_call_stack);
ENGINE_API void engineProfileZoneEnd(engine_profiler_ctx_t* ctx);

// app
ENGINE_API engine_result_code_t engineApplicationCreate(engine_application_t* handle, engine_application_create_desc_t create_desc);
ENGINE_API void engineApplicationDestroy(engine_application_t handle);
ENGINE_API engine_result_code_t engineApplicationSetActive(engine_application_t handle);

// editor
ENGINE_API bool engineEditorIsEnabled();
ENGINE_API bool engineEditorIsPlaying();
ENGINE_API bool engineEditorIsGameView();

// scene
ENGINE_API engine_result_code_t engineSceneCreate(engine_scene_create_desc_t desc, engine_scene_t* out);
ENGINE_API void engineSceneDestroy(engine_scene_t scene);
ENGINE_API engine_result_code_t engineSceneSetActive(engine_scene_t handle);

// game objects in scene
ENGINE_API engine_game_object_t engineGameObjectCreate();
ENGINE_API void                 engineGameObjectDestroy(engine_game_object_t game_object);
ENGINE_API bool                 engineGameObjectIsValid(engine_game_object_t game_object);

// user input hangling
ENGINE_API bool engineKeyboardIsButtonDown(engine_keyboard_keys_t key);
ENGINE_API bool engineKeyboardIsButtonUp(engine_keyboard_keys_t key);

// Relative to the scene: (0, 0) left bottom, (1, 1) right top. While the game doesn't get the mouse (in the editor:
// over another panel or outside the window) it stays at the last position the game got.
ENGINE_API engine_fvec2_t engineMouseCoordsGet();
ENGINE_API bool engineMouseIsButtonDown(engine_mouse_button_t mb);
ENGINE_API bool engineMouseIsButtonUp(engine_mouse_button_t mb);

//frame handling
ENGINE_API engine_application_frame_begine_info_t engineFrameBegin();
ENGINE_API engine_result_code_t                   engineFrameSceneUpdate(float delta_time);
ENGINE_API engine_application_frame_end_info_t    engineFrameEnd();

// pipeline state objects and GPU buffers
ENGINE_API engine_result_code_t engineShaderCreate(const engine_shader_create_desc_t* desc, const char* name, engine_shader_t* out);
ENGINE_API engine_shader_t engineShaderGetByName(const char* name);
ENGINE_API void engineShaderDestroy(engine_shader_t pso);

// fonts
ENGINE_API engine_result_code_t engineFontCreateFromFile(const char* file_name, const char* handle_name);

// geometry: created with the prefab of its model
ENGINE_API engine_geometry_t engineGeometryGetByName(const char* name);
ENGINE_API engine_geometry_attribute_limit_t engineGeometryGetAttributeLimits(engine_geometry_t geometry, engine_vertex_attribute_type_t type);

// textures: creating one gives the caller a reference; a prefab using it holds another, so it is destroyed when the last
// of them is released
ENGINE_API engine_result_code_t engineTexture2DCreateFromFile(const char* file_path, engine_texture_color_space_t color_space, const char* name, engine_texture2d_t* out);
ENGINE_API engine_texture2d_t   engineTextured2DGetByName(const char* name);
ENGINE_API void engineTexture2DRelease(engine_texture2d_t tex2d);

// physics 
ENGINE_API void enginePhysicsSetGravityVector(const float gravity[3]);
ENGINE_API size_t enginePhysicsGetNumCollisions();
ENGINE_API const engine_collision_desc_t* enginePhysicsGetCollisionDesc(size_t idx);
ENGINE_API engine_ray_hit_info_t enginePhysicsRayCast(const engine_game_object_t* ignore_list, size_t ignore_list_count, const engine_ray_t* ray, float max_distance);
ENGINE_API bool enginePhysicsAddForce(engine_game_object_t go, const float force[3], engine_force_type_t type);

// ui
// create data handel first, before loading document!
ENGINE_API engine_result_code_t engineUiDocumentCreateDataHandle(const char* name, const engine_ui_document_data_binding_t* bindings, size_t bindings_count, engine_ui_data_handle_t* out);
ENGINE_API void engineUiDataHandleDestroy(engine_ui_data_handle_t handle);
ENGINE_API void engineUiDataHandleDirtyAllVariables(engine_ui_data_handle_t handle);
ENGINE_API void engineUiDataHandleDirtyVariable(engine_ui_data_handle_t handle, const char* name);

// if document uses data model than creata data model first with function: engineApplicationCreateUiDataHandle(...)
ENGINE_API engine_result_code_t engineUiDocumentCreateFromFile(const char* file_path, engine_ui_document_t* out);
ENGINE_API void engineUiDocumentDestroy(engine_ui_document_t doc);
ENGINE_API void engineUiDocumentShow(engine_ui_document_t ui_doc);
ENGINE_API void engineUiDocumentHide(engine_ui_document_t ui_doc);

// interanlly it caches elements, so it's safe to call it multiple times to get the same object
// however for best performance it's recommeneded to reuse elements and minimize usage of this function
ENGINE_API engine_result_code_t engineUiDocumentGetElementById(engine_ui_document_t document, const char* id, engine_ui_element_t* out);
ENGINE_API engine_result_code_t engineUiElementAddEventCallback(engine_ui_element_t element, engine_ui_event_type_t event_type, void* user_data, void (*callback)(const engine_ui_event_t*, void*));
ENGINE_API engine_result_code_t engineUiElementSetProperty(engine_ui_element_t element, const char* property, const char* value);
ENGINE_API void engineUiElementRemoveProperty(engine_ui_element_t element, const char* property);

// ECS 
ENGINE_API engine_result_code_t engineComponentViewCreate(engine_component_view_t* out);
ENGINE_API void engineComponentViewDestroy(engine_component_view_t view);
ENGINE_API engine_result_code_t engineComponentViewCreateBeginComponentIterator(engine_component_view_t view, engine_component_iterator_t* out);
ENGINE_API engine_result_code_t engineComponentViewCreateEndComponentIterator(engine_component_view_t view, engine_component_iterator_t* out);
ENGINE_API bool engineComponentIteratorCheckEqual(engine_component_iterator_t lhs, engine_component_iterator_t rhs);
ENGINE_API void engineComponentIteratorNext(engine_component_iterator_t iterator);
ENGINE_API engine_game_object_t engineComponentIteratorGetGameObject(engine_component_iterator_t iterator);
ENGINE_API void engineComponentIteratorDelete(engine_component_iterator_t iterator);

// name component
ENGINE_API engine_name_component_t engineGameObjectAddNameComponent(engine_game_object_t game_object);
ENGINE_API engine_name_component_t engineGameObjectGetNameComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateNameComponent(engine_game_object_t game_object, const engine_name_component_t* comp);
ENGINE_API void engineGameObjectRemoveNameComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasNameComponent(engine_game_object_t game_object);
ENGINE_API void engineComponentViewAttachNameComponent(engine_component_view_t view);

// folder component
ENGINE_API engine_folder_component_t engineGameObjectAddFolderComponent(engine_game_object_t game_object);
ENGINE_API engine_folder_component_t engineGameObjectGetFolderComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateFolderComponent(engine_game_object_t game_object, const engine_folder_component_t* comp);
ENGINE_API void engineGameObjectRemoveFolderComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasFolderComponent(engine_game_object_t game_object);

// transform component
ENGINE_API engine_tranform_component_t engineGameObjectAddTransformComponent(engine_game_object_t game_object);
ENGINE_API engine_tranform_component_t engineGameObjectGetTransformComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateTransformComponent(engine_game_object_t game_object, const engine_tranform_component_t* comp);
ENGINE_API void engineGameObjectRemoveTransformComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasTransformComponent(engine_game_object_t game_object);

// light component
ENGINE_API engine_light_component_t engineGameObjectAddLightComponent(engine_game_object_t game_object);
ENGINE_API engine_light_component_t engineGameObjectGetLightComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateLightComponent(engine_game_object_t game_object, const engine_light_component_t* comp);
ENGINE_API void engineGameObjectRemoveLightComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasLightComponent(engine_game_object_t game_object);

// mesh component
ENGINE_API engine_mesh_component_t engineGameObjectAddMeshComponent(engine_game_object_t game_object);
ENGINE_API engine_mesh_component_t engineGameObjectGetMeshComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateMeshComponent(engine_game_object_t game_object, const engine_mesh_component_t* comp);
ENGINE_API void engineGameObjectRemoveMeshComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasMeshComponent(engine_game_object_t game_object);

// skinned mesh component
ENGINE_API engine_skinned_mesh_component_t engineGameObjectAddSkinnedMeshComponent(engine_game_object_t game_object);
ENGINE_API engine_skinned_mesh_component_t engineGameObjectGetSkinnedMeshComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateSkinnedMeshComponent(engine_game_object_t game_object, const engine_skinned_mesh_component_t* comp);
ENGINE_API void engineGameObjectRemoveSkinnedMeshComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasSkinnedMeshComponent(engine_game_object_t game_object);

// skin component
ENGINE_API engine_skin_component_t engineGameObjectAddSkinComponent(engine_game_object_t game_object);
ENGINE_API engine_skin_component_t engineGameObjectGetSkinComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateSkinComponent(engine_game_object_t game_object, const engine_skin_component_t* comp);
ENGINE_API void engineGameObjectRemoveSkinComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasSkinComponent(engine_game_object_t game_object);

// joint attachment component
ENGINE_API engine_joint_attachment_component_t engineGameObjectAddJointAttachmentComponent(engine_game_object_t game_object);
ENGINE_API engine_joint_attachment_component_t engineGameObjectGetJointAttachmentComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateJointAttachmentComponent(engine_game_object_t game_object, const engine_joint_attachment_component_t* comp);
ENGINE_API void engineGameObjectRemoveJointAttachmentComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasJointAttachmentComponent(engine_game_object_t game_object);

// animation controller component
ENGINE_API engine_animation_controller_component_t engineGameObjectAddAnimationControllerComponent(engine_game_object_t game_object);
ENGINE_API engine_animation_controller_component_t engineGameObjectGetAnimationControllerComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateAnimationControllerComponent(engine_game_object_t game_object, const engine_animation_controller_component_t* comp);
ENGINE_API void engineGameObjectRemoveAnimationControllerComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasAnimationControllerComponent(engine_game_object_t game_object);

// material component
ENGINE_API engine_material_component_t engineGameObjectAddMaterialComponent(engine_game_object_t game_object);
ENGINE_API engine_material_component_t engineGameObjectGetMaterialComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateMaterialComponent(engine_game_object_t game_object, const engine_material_component_t* comp);
ENGINE_API void engineGameObjectRemoveMaterialComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasMaterialComponent(engine_game_object_t game_object);

// sprite component
ENGINE_API engine_sprite_component_t engineGameObjectAddSpriteComponent(engine_game_object_t game_object);
ENGINE_API engine_sprite_component_t engineGameObjectGetSpriteComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateSpriteComponent(engine_game_object_t game_object, const engine_sprite_component_t* comp);
ENGINE_API void engineGameObjectRemoveSpriteComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasSpriteComponent(engine_game_object_t game_object);

// camera component
ENGINE_API engine_camera_component_t engineGameObjectAddCameraComponent(engine_game_object_t game_object);
ENGINE_API engine_camera_component_t engineGameObjectGetCameraComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateCameraComponent(engine_game_object_t game_object, const engine_camera_component_t* comp);
ENGINE_API void engineGameObjectRemoveCameraComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasCameraComponent(engine_game_object_t game_object);
ENGINE_API void engineComponentViewAttachCameraComponent(engine_component_view_t view);
ENGINE_API engine_fvec3_t engineCameraComponentConvertWorldPositionToScreenPosition(engine_game_object_t game_object, const float world_pos[3]);
ENGINE_API engine_fvec3_t engineCameraComponentConvertSpacePositionToWorldPosition(engine_game_object_t game_object, const engine_fvec3_t screen_position);

// rigid body component
ENGINE_API engine_rigid_body_component_t engineGameObjectAddRigidBodyComponent(engine_game_object_t game_object);
ENGINE_API engine_rigid_body_component_t engineGameObjectGetRigidBodyComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateRigidBodyComponent(engine_game_object_t game_object, const engine_rigid_body_component_t* comp);
ENGINE_API void engineGameObjectRemoveRigidBodyComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasRigidBodyComponent(engine_game_object_t game_object);

// collider component
ENGINE_API engine_collider_component_t engineGameObjectAddColliderComponent(engine_game_object_t game_object);
ENGINE_API engine_collider_component_t engineGameObjectGetColliderComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateColliderComponent(engine_game_object_t game_object, const engine_collider_component_t* comp);
ENGINE_API void engineGameObjectRemoveColliderComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasColliderComponent(engine_game_object_t game_object);

// parent component
ENGINE_API engine_parent_component_t engineGameObjectAddParentComponent(engine_game_object_t game_object);
ENGINE_API engine_parent_component_t engineGameObjectGetParentComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateParentComponent(engine_game_object_t game_object, const engine_parent_component_t* comp);
ENGINE_API void engineGameObjectRemoveParentComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasParentComponent(engine_game_object_t game_object);

// children: the game objects whose parent component points at the given game object, in the order they were parented.
// It's managed by the engine: parent components are all there is to change. Any change of the hierarchy invalidates
// the indices, so copy the children first if you destroy or reparent them while iterating.
ENGINE_API size_t engineGameObjectGetChildrenCount(engine_game_object_t game_object);
ENGINE_API engine_game_object_t engineGameObjectGetChild(engine_game_object_t game_object, size_t index); // ENGINE_INVALID_GAME_OBJECT_ID when out of range
#ifdef __cplusplus
}
#endif  // #ifndef __cplusplus