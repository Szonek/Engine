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
#include "components/nav_surface_component.h"
#include "components/nav_modifier_component.h"
#include "components/parent_component.h"
#include "components/sprite_component.h"
#include "components/skin_component.h"
#include "components/animation_controller_component.h"
#include "components/character_controller_component.h"

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
    // the longest frame time engineFrameBegin reports, in milliseconds (like Unity's Maximum Allowed Timestep): after a
    // long frame (loading, a breakpoint) the game runs slower instead of jumping ahead. 0 means 1000 / 3.
    float max_delta_time;
} engine_application_create_desc_t;

typedef struct _engine_scene_create_desc_t
{
    uint32_t reserved;  // no options yet; C has no empty structs
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
    // milliseconds since the last frame began, at most engine_application_create_desc_t::max_delta_time
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

// the closest hit of enginePhysicsRayCast
typedef struct _engine_ray_hit_t
{
    engine_game_object_t go;
    float position[3];
    float normal[3];
    float distance;  // from the ray's origin
} engine_ray_hit_t;

// what enginePhysicsRayCast skips; a zero-initialized filter (or none) hits everything but triggers
typedef struct _engine_ray_cast_filter_t
{
    const engine_game_object_t* ignore;  // game objects whose colliders the ray passes through
    size_t ignore_count;
    bool hit_triggers;                   // false: triggers (hitboxes, pickups) are skipped
} engine_ray_cast_filter_t;

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
// holds the variants by value, so it is declared where the type is complete
ENGINE_DECLARE_VECTOR_TYPE(EngineUiDataVariant, engine_ui_data_variant, engine_ui_data_variant_t);

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

// Prefab: an asset built from a model file, named by the file's path relative to the models folder, with '/'
// separators ("characters/Rogue.glb"); the path is its asset ID, so a scene file can load it. Loading it also creates
// the model's geometries, textures, skin and animation set as assets named "<path>#<name>" (e.g.
// "characters/Rogue.glb#Rig", the skin and "characters/Rogue.glb#animations", the animation set); they live as long
// as the prefab. An image in its own file is shared with
// the other models (and textures of the game) using it and lives as long as the last of them.
// Prefabs are reference counted: loading gives the caller a reference to the prefab (loading a model which is loaded
// already returns the same prefab), every instance holds one until it is destroyed, and the prefab is destroyed with
// its assets when the last reference is released. Instantiating it creates the model's game objects in the active
// scene. Components store asset handles without holding references: a game object using a model's asset (e.g. its
// geometry) needs someone to hold the prefab, like the game or an instance.
ENGINE_API engine_result_code_t enginePrefabLoad(engine_model_specification_t spec, const char* path, engine_prefab_t* out);
ENGINE_API engine_prefab_t enginePrefabGetByName(const char* name);
// Returns the root game object of the new instance, or ENGINE_INVALID_GAME_OBJECT_ID. The root is the parent of all the
// other game objects of the instance.
ENGINE_API engine_game_object_t enginePrefabInstantiate(engine_prefab_t prefab);
// Like enginePrefabInstantiate, but every game object of the instance is transient (see engineGameObjectCreateTransient).
ENGINE_API engine_game_object_t enginePrefabInstantiateTransient(engine_prefab_t prefab);
// The prefab the game object is the root of an instance of, or ENGINE_INVALID_OBJECT_HANDLE.
ENGINE_API engine_prefab_t enginePrefabInstanceGetPrefab(engine_game_object_t game_object);
ENGINE_API void enginePrefabRelease(engine_prefab_t prefab);

// Skin: an asset shared by every instance of a skinned model (the skeleton and its binding to the mesh), created with
// the model's prefab and named "<model path>#<skin name>". Each instance's pose belongs to the game object with the
// skin component.
ENGINE_API engine_skin_t engineSkinGetByName(const char* name);

// Animation set: an asset with a model's animation clips compiled for its skin, created with the model's prefab, named
// "<model path>#animations" and shared by every controller of that skin.
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

typedef enum _engine_nav_path_status_t
{
    ENGINE_NAV_PATH_STATUS_COMPLETE = 0, // the path ends at the end
    ENGINE_NAV_PATH_STATUS_PARTIAL = 1, // the end can't be reached: the path ends at the reachable point closest to it
} engine_nav_path_status_t;

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
// Scene files (JSON) hold the authored content of a scene: every game object with a persistent ID (transient ones are
// never saved) and its components which hold authored data (name, folder, transform, parent, mesh, material, light,
// nav surface and modifier, rigid body, collider, character controller, and the game-defined components, see
// engineComponentTypeRegister), with assets referenced by asset ID and game objects by persistent ID. A path is relative to assets/scenes, or absolute.
// Saves the active scene; fails when the file can't be written.
ENGINE_API engine_result_code_t engineSceneSave(const char* path);
// Adds the game objects of a scene file to the active scene, with the IDs of the file (one whose ID is used already
// gets a new one). The models and images it references are loaded when needed and live at least as long as the scene.
// Fails, without changing the scene, when the file can't be read or isn't a scene file this engine can read; problems
// inside a scene file (an unknown component, a missing asset, ...) are logged and the rest is loaded.
ENGINE_API engine_result_code_t engineSceneLoad(const char* path);

// Game-defined components: plain C structs the game describes field by field (like the engine's own components), so
// scene files save them and the editor shows and edits them without code for them. Register a type once, after the
// application is created and before a scene file holding it is loaded; types can't be unregistered. A type is a
// trivially copyable C struct: no pointers, no std::string (a fixed char array instead), handles of game objects and
// assets as their fields (they are translated to persistent IDs / asset IDs in a file).
typedef uint32_t engine_component_type_t;  // ENGINE_INVALID_OBJECT_HANDLE for none

typedef enum _engine_field_kind_t
{
    ENGINE_FIELD_KIND_BOOL = 0,
    ENGINE_FIELD_KIND_INT32,
    ENGINE_FIELD_KIND_UINT32,
    ENGINE_FIELD_KIND_FLOAT,
    // a char array of `count` chars, null terminated
    ENGINE_FIELD_KIND_STRING,
    // a C enum (int sized), saved by the name of its value (enum_values)
    ENGINE_FIELD_KIND_ENUM,
    // a struct of the type `type_name`, registered before with engineStructTypeRegister
    ENGINE_FIELD_KIND_STRUCT,
    // a union whose case is selected by the enum field `union_tag` described before it; only the active case is saved
    ENGINE_FIELD_KIND_UNION,
    // an engine_geometry_t / engine_texture2d_t, saved as its asset ID
    ENGINE_FIELD_KIND_GEOMETRY,
    ENGINE_FIELD_KIND_TEXTURE,
    // an engine_game_object_t, saved as the persistent ID of the game object
    ENGINE_FIELD_KIND_GAME_OBJECT,
    ENGINE_FIELD_KIND_COUNT
} engine_field_kind_t;

typedef struct _engine_enum_value_desc_t
{
    const char* name;
    int32_t value;
} engine_enum_value_desc_t;

typedef struct _engine_union_case_desc_t
{
    // the value of the union's tag selecting this case
    int32_t tag;
    // a struct type registered before with engineStructTypeRegister
    const char* type_name;
} engine_union_case_desc_t;

typedef struct _engine_field_desc_t
{
    const char* name;
    engine_field_kind_t kind;
    // offsetof(the struct, the field)
    uint32_t offset;
    // 0 for one value, N for a fixed size array (float[3]) saved as a JSON array; for ENGINE_FIELD_KIND_STRING the size
    // of the char array (terminator included)
    uint32_t count;
    // ENGINE_FIELD_KIND_ENUM
    const engine_enum_value_desc_t* enum_values;
    uint32_t enum_values_count;
    // ENGINE_FIELD_KIND_STRUCT
    const char* type_name;
    // ENGINE_FIELD_KIND_UNION: the union's size is the size of its biggest case
    const char* union_tag;
    const engine_union_case_desc_t* union_cases;
    uint32_t union_cases_count;
} engine_field_desc_t;

typedef struct _engine_type_desc_t
{
    // unique among all types, the engine's included (e.g. "enemy_config"); the name scene files use
    const char* name;
    // saved in scene files; a file holding a newer version than the registered one doesn't load the component. 0 is 1.
    uint32_t version;
    // sizeof / alignof the struct; a component is at most ENGINE_COMPONENT_TYPE_MAX_SIZE bytes, aligned to at most 16
    uint32_t size;
    uint32_t alignment;
    // the value a new component gets (engineGameObjectAddDataComponent with no value, fields missing in a file), or
    // NULL for zeroes; not used by struct types
    const void* default_value;
    // every field: a field left out is never saved or shown
    const engine_field_desc_t* fields;
    uint32_t fields_count;
} engine_type_desc_t;

#define ENGINE_COMPONENT_TYPE_MAX_SIZE 256

// Registers a component type. Fails (logged) when the description has a mistake: a name used already, a field outside
// the struct, overlapping or misaligned fields, an unknown struct type, ...
ENGINE_API engine_result_code_t engineComponentTypeRegister(const engine_type_desc_t* desc, engine_component_type_t* out);
// Registers a struct type used inside components (ENGINE_FIELD_KIND_STRUCT, union cases), not a component itself.
ENGINE_API engine_result_code_t engineStructTypeRegister(const engine_type_desc_t* desc);
// the component type with this name, also one of the engine's (e.g. "transform"); ENGINE_INVALID_OBJECT_HANDLE when none
ENGINE_API engine_component_type_t engineComponentTypeGetByName(const char* name);
// Components of a registered type on game objects of the active scene. Values are copies of the whole struct (`size`
// bytes of the type), like the engine's components: changes take effect with Update. Add fails when the game object
// has the component already, Get / Update when it doesn't have it.
// value: the component's value, or NULL for the type's default value
ENGINE_API engine_result_code_t engineGameObjectAddDataComponent(engine_game_object_t game_object, engine_component_type_t type, const void* value);
ENGINE_API engine_result_code_t engineGameObjectGetDataComponent(engine_game_object_t game_object, engine_component_type_t type, void* out);
ENGINE_API engine_result_code_t engineGameObjectUpdateDataComponent(engine_game_object_t game_object, engine_component_type_t type, const void* value);
ENGINE_API void engineGameObjectRemoveDataComponent(engine_game_object_t game_object, engine_component_type_t type);
ENGINE_API bool engineGameObjectHasDataComponent(engine_game_object_t game_object, engine_component_type_t type);

// game objects in scene
ENGINE_API engine_game_object_t engineGameObjectCreate();
// A game object which is not part of the scene's content: it gets no persistent ID (so it doesn't shift the IDs of the
// others) and is never saved. For objects spawned at runtime, e.g. by a script.
ENGINE_API engine_game_object_t engineGameObjectCreateTransient();
// false for an invalid game object
ENGINE_API bool                 engineGameObjectIsTransient(engine_game_object_t game_object);
// destroys the game object and its children (with theirs); reparent a child first to keep it
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
// The closest collider hit along origin + direction * t, t in [0, max_distance] (direction need not be normalized; for a
// segment from a to b pass b - a and its length). Sees this frame's state, like every physics query. filter may be NULL
// (everything but triggers). Returns true and fills out_hit on a hit; false on a miss, for an empty ray or no active scene.
ENGINE_API bool enginePhysicsRayCast(const float origin[3], const float direction[3], float max_distance, const engine_ray_cast_filter_t* filter, engine_ray_hit_t* out_hit);
ENGINE_API bool enginePhysicsAddForce(engine_game_object_t go, const float force[3], engine_force_type_t type);

// navigation: the scene's nav mesh is built from its static colliders with the settings of its nav surface component
// (see engine_nav_surface_component_t, engine_nav_modifier_component_t). It is derived data, never saved: build it after
// the level is built or loaded, and again after its static colliders changed. The build logs its triangle count and time.
ENGINE_API engine_result_code_t engineNavMeshBuild();
// The path from start to end over the nav mesh, both snapped onto it (the nav mesh is searched within the box of
// search_half_extents around each): its corners as x, y, z triples, the first one the start. *out_status says whether it
// reaches the end or, when the end can't be reached (e.g. on a disconnected island), the reachable point closest to it.
// A path with more than max_points corners is cut off after the first max_points. Fails (*out_points_count = 0) when the
// scene has no nav mesh or either point is off it.
ENGINE_API engine_result_code_t engineNavFindPath(const float start[3], const float end[3], const float search_half_extents[3],
    float* out_points, size_t max_points, size_t* out_points_count, engine_nav_path_status_t* out_status);
// The closest point on the nav mesh within the box of search_half_extents around `point`. Fails when the scene has no nav
// mesh or there is none.
ENGINE_API engine_result_code_t engineNavFindNearestPoint(const float point[3], const float search_half_extents[3], float out[3]);

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
ENGINE_API engine_transform_component_t engineGameObjectAddTransformComponent(engine_game_object_t game_object);
ENGINE_API engine_transform_component_t engineGameObjectGetTransformComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateTransformComponent(engine_game_object_t game_object, const engine_transform_component_t* comp);
ENGINE_API void engineGameObjectRemoveTransformComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasTransformComponent(engine_game_object_t game_object);
// The game object's local-to-world matrix (column-major): its transform, joint attachment and parents. Derived by the
// scene and read-only, valid after the last scene update (engineFrameSceneUpdate): changes made since then show up after the next one.
// Fails when the game object has no transform component.
ENGINE_API engine_result_code_t engineGameObjectGetWorldTransform(engine_game_object_t game_object, float out_matrix[16]);

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

// character controller component (see engine_character_controller_component_t)
ENGINE_API engine_character_controller_component_t engineGameObjectAddCharacterControllerComponent(engine_game_object_t game_object);
ENGINE_API engine_character_controller_component_t engineGameObjectGetCharacterControllerComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateCharacterControllerComponent(engine_game_object_t game_object, const engine_character_controller_component_t* comp);
ENGINE_API void engineGameObjectRemoveCharacterControllerComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasCharacterControllerComponent(engine_game_object_t game_object);
// Moves a character by velocity * delta_time (m/s, all three axes, e.g. a jump) plus its fall: it keeps a vertical speed
// from the physics' gravity between moves, which it loses on walkable ground or under a ceiling. The capsule collides
// with static and kinematic bodies (see engine_character_controller_component_t); the transform component is updated
// right away. Call it once per frame while the character walks or falls; while it doesn't (e.g. a scripted climb), its
// transform moves it. Fails without the component or for a game object with a parent. out_result may be NULL.
ENGINE_API engine_result_code_t engineCharacterControllerMove(engine_game_object_t game_object, const float velocity[3], float delta_time, engine_character_controller_move_result_t* out_result);

// collider component
ENGINE_API engine_collider_component_t engineGameObjectAddColliderComponent(engine_game_object_t game_object);
ENGINE_API engine_collider_component_t engineGameObjectGetColliderComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateColliderComponent(engine_game_object_t game_object, const engine_collider_component_t* comp);
ENGINE_API void engineGameObjectRemoveColliderComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasColliderComponent(engine_game_object_t game_object);

// nav surface component: one per scene
ENGINE_API engine_nav_surface_component_t engineGameObjectAddNavSurfaceComponent(engine_game_object_t game_object);
ENGINE_API engine_nav_surface_component_t engineGameObjectGetNavSurfaceComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateNavSurfaceComponent(engine_game_object_t game_object, const engine_nav_surface_component_t* comp);
ENGINE_API void engineGameObjectRemoveNavSurfaceComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasNavSurfaceComponent(engine_game_object_t game_object);

// nav modifier component
ENGINE_API engine_nav_modifier_component_t engineGameObjectAddNavModifierComponent(engine_game_object_t game_object);
ENGINE_API engine_nav_modifier_component_t engineGameObjectGetNavModifierComponent(engine_game_object_t game_object);
ENGINE_API void engineGameObjectUpdateNavModifierComponent(engine_game_object_t game_object, const engine_nav_modifier_component_t* comp);
ENGINE_API void engineGameObjectRemoveNavModifierComponent(engine_game_object_t game_object);
ENGINE_API bool engineGameObjectHasNavModifierComponent(engine_game_object_t game_object);

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