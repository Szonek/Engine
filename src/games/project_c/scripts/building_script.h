#pragma once
#include "base_script.h"
#include "game_components.h"

#include <optional>
#include <vector>

#include <glm/glm.hpp>

namespace project_c
{
class AppProjectC;

// Static piece of a building (wall, roof or an invisible collision block): a game object with a collider (or a mesh
// only) and a building_part_config component saying how traversal and the house treat it.
// Player traversal (climb/vault/mantle) only reacts to colliders owned by BuildingParts.
class BuildingPart : public BaseNode
{
public:
    BuildingPart(engine::IScene* my_scene, engine_game_object_t go);

    // its designer-set values (the building_part_config component), read on every use
    BuildingPartConfig config() const;

    // surface_normal - normal of the face player wants to climb
    bool is_climbable(const glm::vec3& surface_normal) const;
    // Point traversal should align to, i.e. center of the window opening for window frame parts.
    std::optional<glm::vec3> get_alignment_point() const;
    BuildingPartHide get_hide_when_inside() const { return config().hide_when_inside; }

    // Hides mesh, and optionally removes the collider (so mouse picking goes through hidden roofs).
    void set_hidden(bool hidden, bool affect_collider);

private:
    bool hidden_ = false;
    // the collider while it is removed, to add it back
    std::optional<engine_collider_component_t> removed_collider_;
};

// Small, single room house: 4 walls (one with an open window), walkable roof. The house's game object stands at the
// center of its floor; its parts are BuildingParts which are its children (level content, see build_house). While the
// player is inside, the house hides the parts marked with a hide_when_inside (the roof and the wall facing the camera)
// and the props on the roof (the house_config component).
class House : public BaseNode
{
public:
    static constexpr float half_size = 2.0f;      // distance from house center to wall centerline
    static constexpr float wall_height = 2.0f;
    static constexpr float wall_thickness = 0.5f;
    static constexpr float roof_thickness = 0.25f;

    House(engine::IScene* my_scene, engine_game_object_t go);

    void on_start() override;
    void update(float dt) override;

    // world position of the center of the roof's top
    glm::vec3 get_roof_center() const;
    // Adds a game object standing on the roof (house_config), hidden together with the roof; level authoring. False
    // (logged) when the house has as many as it can hold.
    bool add_roof_prop(engine_game_object_t go);

private:
    // the center of the house's floor
    glm::vec3 get_position() const;
    bool is_inside(const glm::vec3& p) const;

private:
    bool player_inside_ = false;
    engine_game_object_t player_go_ = ENGINE_INVALID_GAME_OBJECT_ID;
    // child parts with a hide_when_inside other than never, found in on_start
    std::vector<BuildingPart*> hidden_when_inside_;
};

// A house centered on `center` (x, z) standing on ground_y, with its parts; parts whose prefab failed to load are skipped.
House* build_house(engine::IScene& scene, AppProjectC& app, const glm::vec2& center, float ground_y);

} // namespace project_c
