#pragma once
#include "base_script.h"

#include <optional>
#include <vector>

#include <glm/glm.hpp>

namespace project_c
{
class AppProjectC;

// Static piece of a building (wall, roof or an invisible collision block).
// Player traversal (climb/vault/mantle) only reacts to colliders owned by BuildingParts.
class BuildingPart : public BaseNode
{
public:
    struct Box
    {
        glm::vec3 center;        // in the part's local (unscaled) space
        glm::vec3 half_extents;  // in the part's local (unscaled) space
    };

    // go is either an instantiated prefab (visible piece) or a fresh game object (invisible blocker).
    // Transform (position, y rotation, scale) is applied to the game object, so collider box is defined in mesh space.
    BuildingPart(engine::IScene* my_scene, engine_game_object_t go, std::string_view name,
        const glm::vec3& position, float y_rotation_degrees, const glm::vec3& scale,
        std::optional<Box> collider, bool climbable);

    // surface_normal - normal of the face player wants to climb
    bool is_climbable(const glm::vec3& surface_normal) const;
    // Only faces pointing away from this point are climbable, i.e. house walls can be climbed only from the outside.
    void set_climbable_away_from(const glm::vec3& point) { climbable_away_from_ = point; }

    // Point traversal should align to, i.e. center of the window opening for window frame parts.
    void set_alignment_point(const glm::vec3& point) { alignment_point_ = point; }
    std::optional<glm::vec3> get_alignment_point() const { return alignment_point_; }

    // Hides mesh, and optionally removes the collider (so mouse picking goes through hidden roofs).
    void set_hidden(bool hidden, bool affect_collider);

private:
    void add_collider();

private:
    std::optional<Box> collider_;
    std::optional<glm::vec3> alignment_point_;
    std::optional<glm::vec3> climbable_away_from_;
    glm::vec3 position_{ 0.0f };
    bool climbable_ = false;
    bool hidden_ = false;
    bool collider_removed_ = false;
};

// Small, single room house: 4 walls (one with an open window), walkable roof.
// While the player is inside, the roof and the wall facing the camera are hidden.
class House : public BaseNode
{
public:
    static constexpr float half_size = 2.0f;      // distance from house center to wall centerline
    static constexpr float wall_height = 2.0f;
    static constexpr float wall_thickness = 0.5f;
    static constexpr float roof_thickness = 0.25f;

    House(engine::IScene* my_scene, AppProjectC& app, const glm::vec2& center, float ground_y);

    void update(float dt) override;

    float get_roof_top_y() const { return ground_y_ + wall_height + roof_thickness; }
    glm::vec3 get_roof_center() const { return { center_.x, get_roof_top_y(), center_.y }; }
    // objects standing on the roof, hidden together with the roof
    void add_roof_prop(engine_game_object_t go) { roof_props_.push_back(go); }

private:
    bool is_inside(const glm::vec3& p) const;

    void add_wall(AppProjectC& app, const glm::vec3& position, float y_rotation_degrees, const glm::vec3& scale, bool camera_facing);
    void add_window_wall(AppProjectC& app, const glm::vec3& position, float y_rotation_degrees, const glm::vec3& scale, bool camera_facing);
    void add_roof(AppProjectC& app);

private:
    glm::vec2 center_;
    float ground_y_ = 0.0f;
    bool player_inside_ = false;
    engine_game_object_t player_go_ = ENGINE_INVALID_GAME_OBJECT_ID;
    std::vector<BuildingPart*> camera_occluders_;
    std::vector<engine_game_object_t> roof_props_;
    BuildingPart* roof_ = nullptr;
};

} // namespace project_c
