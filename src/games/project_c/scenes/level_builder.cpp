#include "level_builder.h"

#include "../app.h"
#include "../scripts/building_script.h"
#include "../scripts/camera_script.h"
#include "../scripts/encounter_script.h"
#include "../scripts/enemy_script.h"
#include "../scripts/enviorment_script.h"
#include "../scripts/game_components.h"
#include "../scripts/gameplay_hud_script.h"
#include "../scripts/interactable_script.h"
#include "../scripts/level_exit_script.h"
#include "../scripts/player_script.h"
#include "../scripts/scripts_utils.h"

#include <iscene.h>
#include <utils.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>

namespace
{
using project_c::utils::copy_string;
using project_c::utils::set_name;

// lists the game object in the given folder of the editor's scene hierarchy
void set_editor_folder(engine_game_object_t go, std::string_view path)
{
    auto fc = engineGameObjectAddFolderComponent(go);
    copy_string(fc.path, path);
    engineGameObjectUpdateFolderComponent(go, &fc);
}

void set_position_and_scale(engine_game_object_t go, const glm::vec3& position, float scale)
{
    auto tc = engineGameObjectHasTransformComponent(go) ? engineGameObjectGetTransformComponent(go) : engineGameObjectAddTransformComponent(go);
    set_c_array(tc.position, std::array<float, 3>{ position.x, position.y, position.z });
    set_c_array(tc.scale, std::array<float, 3>{ scale, scale, scale });
    engineGameObjectUpdateTransformComponent(go, &tc);
}

// a character is moved by its character controller; sizes in metres, in the character's scaled space
void add_character_controller(engine_game_object_t go, float radius, float model_scale)
{
    auto character = engineGameObjectAddCharacterControllerComponent(go);
    character.radius = radius / model_scale;
    character.height = 0.7f / model_scale;
    character.step_height = 0.15f / model_scale;
    character.max_slope = 45.0f;
    engineGameObjectUpdateCharacterControllerComponent(go, &character);
}

// a compound collider with one box child (half extents), in the game object's space
void add_box_collider(engine_game_object_t go, const glm::vec3& center, const glm::vec3& half_extents)
{
    auto cc = engineGameObjectAddColliderComponent(go);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    auto& child = cc.collider.compound.children[0];
    child.type = ENGINE_COLLIDER_TYPE_BOX;
    child.rotation_quaternion[3] = 1.0f;
    set_c_array(child.transform, std::array<float, 3>{ center.x, center.y, center.z });
    set_c_array(child.collider.box.size, std::array<float, 3>{ half_extents.x, half_extents.y, half_extents.z });
    engineGameObjectUpdateColliderComponent(go, &cc);
}
}  // namespace

project_c::LevelBuilder::LevelBuilder(AppProjectC& app, engine::IScene& scene)
    : app_(app)
    , scene_(scene)
{
}

project_c::CameraScript* project_c::LevelBuilder::camera()
{
    const auto go = engineGameObjectCreate();
    set_name(go, "camera");
    CameraScript::add_components(go);
    return scene_.attach_script<CameraScript>(go);
}

engine_game_object_t project_c::LevelBuilder::main_light()
{
    const auto go = engineGameObjectCreate();
    set_name(go, "main-light");
    set_editor_folder(go, "lights");
    set_position_and_scale(go, glm::vec3(0.0f, 10.0f, 0.0f), 0.1f);

    // a small cube shows where it is
    const auto cube_geo = engineGeometryGetByName("cube.glb");
    if (ENGINE_INVALID_OBJECT_HANDLE != cube_geo)
    {
        auto mc = engineGameObjectAddMeshComponent(go);
        mc.geometry = cube_geo;
        engineGameObjectUpdateMeshComponent(go, &mc);
    }
    auto mat = engineGameObjectAddMaterialComponent(go);
    engineGameObjectUpdateMaterialComponent(go, &mat);

    auto lc = engineGameObjectAddLightComponent(go);
    lc.type = ENGINE_LIGHT_TYPE_DIRECTIONAL;
    // Shadows remove only the diffuse and specular part of the light, so most of the light has to be diffuse to see them.
    set_c_array(lc.intensity.ambient, std::array<float, 3>{ 0.45f, 0.45f, 0.45f });
    set_c_array(lc.intensity.diffuse, std::array<float, 3>{ 0.75f, 0.75f, 0.75f });
    set_c_array(lc.intensity.specular, std::array<float, 3>{ 0.1f, 0.1f, 0.1f });
    // direction towards the light. Tilted, so shadows are not hidden right under the objects.
    set_c_array(lc.directional.direction, std::array<float, 3>{ 0.4f, 1.0f, 0.3f });
    engineGameObjectUpdateLightComponent(go, &lc);
    return go;
}

project_c::Encounter* project_c::LevelBuilder::encounter()
{
    const auto go = engineGameObjectCreate();
    set_name(go, "encounter");
    return scene_.attach_script<Encounter>(go);
}

project_c::GameplayHud* project_c::LevelBuilder::hud()
{
    const auto go = engineGameObjectCreate();
    set_name(go, "gameplay-hud");
    return scene_.attach_script<GameplayHud>(go);
}

engine_game_object_t project_c::LevelBuilder::nav_surface()
{
    const auto go = engineGameObjectCreate();
    set_name(go, "nav-surface");
    // the engine's defaults fit the skeletons
    engineGameObjectAddNavSurfaceComponent(go);
    return go;
}

project_c::LevelBuilder& project_c::LevelBuilder::floor(float x, float z)
{
    place_floor(x, z);
    return *this;
}

engine_game_object_t project_c::LevelBuilder::place_floor(float x, float z)
{
    if (!app_.is_prefab_available(PREFAB_TYPE_FLOOR))
    {
        return ENGINE_INVALID_GAME_OBJECT_ID;
    }
    const auto go = app_.instantiate_prefab(PREFAB_TYPE_FLOOR).go;
    set_name(go, "floor");
    set_editor_folder(go, "environment/floor");
    // on the cell, at the model's height
    const auto tc = engineGameObjectGetTransformComponent(go);
    set_position_and_scale(go, glm::vec3(tc.position[0] + x, tc.position[1], tc.position[2] + z), 0.25f);
    add_box_collider(go, glm::vec3(0.0f), glm::vec3(2.0f, 0.5f, 2.0f));
    scene_.attach_script<Floor>(go);
    return go;
}

project_c::LevelBuilder& project_c::LevelBuilder::floor_area(float x_min, float z_min, float x_max, float z_max)
{
    for (auto x = x_min; x <= x_max; x += 1.0f)
    {
        for (auto z = z_min; z <= z_max; z += 1.0f)
        {
            floor(x, z);
        }
    }
    return *this;
}

project_c::LevelBuilder& project_c::LevelBuilder::floor_not_walkable(float x, float z)
{
    const auto tile = place_floor(x, z);
    if (tile != ENGINE_INVALID_GAME_OBJECT_ID)
    {
        auto nmc = engineGameObjectAddNavModifierComponent(tile);
        nmc.type = ENGINE_NAV_MODIFIER_TYPE_NOT_WALKABLE;
        engineGameObjectUpdateNavModifierComponent(tile, &nmc);
    }
    return *this;
}

project_c::LevelBuilder& project_c::LevelBuilder::floor_outside_region(float x, float z)
{
    if (app_.is_prefab_available(PREFAB_FLOOR_OUTSIDE_REGION))
    {
        const auto go = app_.instantiate_prefab(PREFAB_FLOOR_OUTSIDE_REGION).go;
        set_name(go, "primitive-cube");
        set_editor_folder(go, "environment/primitive-cube");
        set_position_and_scale(go, glm::vec3(x, 0.0f, z), 0.25f);
    }
    return *this;
}

project_c::LevelBuilder& project_c::LevelBuilder::wall(float x, float z, float y_rotation_degrees)
{
    if (!app_.is_prefab_available(PREFAB_TYPE_WALL))
    {
        return *this;
    }
    const auto go = app_.instantiate_prefab(PREFAB_TYPE_WALL).go;
    set_name(go, "wall");
    set_editor_folder(go, "environment/wall");
    auto tc = engineGameObjectGetTransformComponent(go);
    tc.position[0] += x;
    tc.position[2] += z;
    set_c_array(tc.scale, std::array<float, 3>{ 0.25f, 0.25f, 0.25f });
    const glm::quat rot = glm::angleAxis(glm::radians(y_rotation_degrees), glm::vec3(0.0f, 1.0f, 0.0f));
    set_c_array(tc.rotation, std::array<float, 4>{ rot.x, rot.y, rot.z, rot.w });
    engineGameObjectUpdateTransformComponent(go, &tc);
    add_box_collider(go, glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(2.0f, 2.0f, 0.5f));
    return *this;
}

engine_game_object_t project_c::LevelBuilder::obstacle(const glm::vec3& center, const glm::vec3& size)
{
    const auto go = engineGameObjectCreate();
    set_name(go, "obstacle");

    auto tc = engineGameObjectAddTransformComponent(go);
    tc.position[0] = center.x;
    tc.position[1] = center.y;
    tc.position[2] = center.z;
    engineGameObjectUpdateTransformComponent(go, &tc);

    // no rigid body: a static collider
    auto cc = engineGameObjectAddColliderComponent(go);
    cc.type = ENGINE_COLLIDER_TYPE_BOX;
    // box sizes are half extents
    cc.collider.box.size[0] = size.x * 0.5f;
    cc.collider.box.size[1] = size.y * 0.5f;
    cc.collider.box.size[2] = size.z * 0.5f;
    engineGameObjectUpdateColliderComponent(go, &cc);
    return go;
}

project_c::Player* project_c::LevelBuilder::player(float x, float z)
{
    if (!app_.is_prefab_available(PREFAB_TYPE_ROGUE))
    {
        return nullptr;
    }
    return set_up_player(app_.instantiate_prefab(PREFAB_TYPE_ROGUE).go, glm::vec3(x, 1.0f, z), glm::identity<glm::quat>());
}

project_c::Player* project_c::LevelBuilder::spawn_player(engine_game_object_t player_start)
{
    // where the start is in the world now (it may be parented, e.g. under a room), not as of the last scene update:
    // a level spawns the player right after loading
    float world[16] = {};
    if (!app_.is_prefab_available(PREFAB_TYPE_ROGUE) || engineGameObjectComputeWorldTransform(player_start, world) != ENGINE_RESULT_CODE_OK)
    {
        return nullptr;
    }
    const auto matrix = glm::make_mat4(world);
    // the start's facing, without its scale
    const glm::mat3 rotation(glm::normalize(glm::vec3(matrix[0])), glm::normalize(glm::vec3(matrix[1])), glm::normalize(glm::vec3(matrix[2])));
    return set_up_player(app_.spawn_prefab(PREFAB_TYPE_ROGUE).go, glm::vec3(matrix[3]), glm::quat_cast(rotation));
}

project_c::Player* project_c::LevelBuilder::set_up_player(engine_game_object_t go, const glm::vec3& position, const glm::quat& rotation)
{
    set_name(go, "player");
    set_position_and_scale(go, position, Player::MODEL_SCALE);
    auto tc = engineGameObjectGetTransformComponent(go);
    set_c_array(tc.rotation, std::array<float, 4>{ rotation.x, rotation.y, rotation.z, rotation.w });
    engineGameObjectUpdateTransformComponent(go, &tc);
    add_character_controller(go, 0.15f, Player::MODEL_SCALE);
    auto config = default_player_config();
    config.tuning = app_.get_player_tuning();
    engineGameObjectAddDataComponent(go, game_components().player_config, &config);
    return scene_.attach_script<Player>(go);
}

engine_game_object_t project_c::LevelBuilder::player_start(float x, float z, float yaw_degrees, std::string_view id)
{
    const auto go = engineGameObjectCreate();
    set_name(go, "player-start");
    set_editor_folder(go, "player_starts");
    set_position_and_scale(go, glm::vec3(x, 1.0f, z), 1.0f);
    auto tc = engineGameObjectGetTransformComponent(go);
    const auto rotation = glm::angleAxis(glm::radians(yaw_degrees), glm::vec3(0.0f, 1.0f, 0.0f));
    set_c_array(tc.rotation, std::array<float, 4>{ rotation.x, rotation.y, rotation.z, rotation.w });
    engineGameObjectUpdateTransformComponent(go, &tc);
    auto ps = default_player_start();
    if (!copy_string(ps.id, id))
    {
        log(fmt::format("The player start id \"{}\" is longer than {} characters; it is cut.\n", id, PLAYER_START_ID_MAX_LENGTH - 1));
    }
    engineGameObjectAddDataComponent(go, game_components().player_start, &ps);
    return go;
}

project_c::LevelExit* project_c::LevelBuilder::level_exit(float x, float z, std::string_view level, std::string_view entrance)
{
    const auto go = engineGameObjectCreate();
    set_name(go, "level-exit");
    set_editor_folder(go, "level_exits");
    set_position_and_scale(go, glm::vec3(x, 0.0f, z), 1.0f);
    // A trigger standing on the floor. Kinematic: static colliders never detect the player's capsule, which is
    // kinematic itself (kinematic bodies pair with dynamic and kinematic ones only).
    add_box_collider(go, glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.5f, 0.5f, 0.5f));
    auto cc = engineGameObjectGetColliderComponent(go);
    cc.is_trigger = true;
    engineGameObjectUpdateColliderComponent(go, &cc);
    auto rb = engineGameObjectAddRigidBodyComponent(go);
    rb.type = ENGINE_RIGID_BODY_TYPE_KINEMATIC;
    engineGameObjectUpdateRigidBodyComponent(go, &rb);

    // the pad showing it: a thin coloured slab on the floor (the traversal marker's cube: 2 units, pivot at the bottom)
    const auto cube_geo = engineGeometryGetByName(app_.model_asset_id(PREFAB_TYPE_TRAVERSAL_MARKER, "Cube.16427").c_str());
    if (ENGINE_INVALID_OBJECT_HANDLE != cube_geo)
    {
        const auto pad = engineGameObjectCreate();
        set_name(pad, "level-exit-pad");
        auto tc = engineGameObjectAddTransformComponent(pad);
        // on the floor tiles' top
        set_c_array(tc.position, std::array<float, 3>{ 0.0f, 0.125f, 0.0f });
        set_c_array(tc.scale, std::array<float, 3>{ 0.45f, 0.01f, 0.45f });
        engineGameObjectUpdateTransformComponent(pad, &tc);
        auto pc = engineGameObjectAddParentComponent(pad);
        pc.parent = go;
        engineGameObjectUpdateParentComponent(pad, &pc);
        auto mc = engineGameObjectAddMeshComponent(pad);
        mc.geometry = cube_geo;
        engineGameObjectUpdateMeshComponent(pad, &mc);
        // a flat colour, no texture
        auto mat = engineGameObjectAddMaterialComponent(pad);
        set_c_array(mat.diffuse_color, std::array<float, 4>{ 0.25f, 0.8f, 1.0f, 1.0f });
        engineGameObjectUpdateMaterialComponent(pad, &mat);
    }

    auto exit = default_level_exit();
    if (!copy_string(exit.level, level))
    {
        log(fmt::format("The level exit's level \"{}\" is longer than {} characters; it is cut and leads nowhere.\n", level,
            LEVEL_PATH_MAX_LENGTH - 1));
    }
    if (!copy_string(exit.entrance, entrance))
    {
        log(fmt::format("The level exit's entrance \"{}\" is longer than {} characters; it is cut.\n", entrance,
            PLAYER_START_ID_MAX_LENGTH - 1));
    }
    engineGameObjectAddDataComponent(go, game_components().level_exit, &exit);
    return scene_.attach_script<LevelExit>(go);
}

project_c::Enemy* project_c::LevelBuilder::enemy(float x, float z)
{
    if (!app_.is_prefab_available(PREFAB_TYPE_SKELETON_WARRIOR))
    {
        return nullptr;
    }
    const auto go = app_.instantiate_prefab(PREFAB_TYPE_SKELETON_WARRIOR).go;
    set_name(go, "enemy");
    const auto tc = engineGameObjectGetTransformComponent(go);
    set_position_and_scale(go, glm::vec3(x, tc.position[1] + 0.25f, z), Enemy::MODEL_SCALE);
    add_character_controller(go, 0.18f, Enemy::MODEL_SCALE);
    auto config = default_enemy_config();
    config.type = app_.get_skeleton_warrior_type();
    engineGameObjectAddDataComponent(go, game_components().enemy_config, &config);
    return scene_.attach_script<Enemy>(go);
}

project_c::Chest* project_c::LevelBuilder::chest(float x, float y, float z)
{
    if (!app_.is_prefab_available(PREFAB_TYPE_BOX))
    {
        return nullptr;
    }
    const auto go = app_.instantiate_prefab(PREFAB_TYPE_BOX).go;
    set_name(go, "chest");
    auto tc = engineGameObjectGetTransformComponent(go);
    set_c_array(tc.position, std::array<float, 3>{ x, y, z });
    engineGameObjectUpdateTransformComponent(go, &tc);
    add_box_collider(go, glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.4f, 0.2f, 0.3f));
    return scene_.attach_script<Chest>(go);
}

project_c::Weapon* project_c::LevelBuilder::weapon_on_ground(float x, float z)
{
    // weapons reuse geometry and texture of the rogue asset
    if (!app_.is_prefab_available(PREFAB_TYPE_ROGUE))
    {
        return nullptr;
    }
    const auto go = engineGameObjectCreate();
    Weapon::add_components(go, app_);
    Weapon::add_lying_components(go, glm::vec3(x, 0.5f, z));
    return scene_.attach_script<Weapon>(go);
}

project_c::House* project_c::LevelBuilder::house(float x, float z, float ground_y)
{
    return build_house(scene_, app_, glm::vec2(x, z), ground_y);
}
