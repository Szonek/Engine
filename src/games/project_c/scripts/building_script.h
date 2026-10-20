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

    // What the House hides while the player is inside it (the camera facing walls, the roof)
    enum class HideWhenInside
    {
        eNever,
        eMesh,
        eMeshAndCollider  // so mouse picking goes through the hidden roof
    };
    void set_hide_when_inside(HideWhenInside hide) { hide_when_inside_ = hide; }
    HideWhenInside get_hide_when_inside() const { return hide_when_inside_; }

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
    HideWhenInside hide_when_inside_ = HideWhenInside::eNever;
    bool hidden_ = false;
    bool collider_removed_ = false;
};

// Small, single room house: 4 walls (one with an open window), walkable roof.
// The parts are level content: BuildingParts which are children of the house (build_house places them). While the
// player is inside, the house hides the parts marked with HideWhenInside (the roof and the wall facing the camera).
class House : public BaseNode
{
public:
    static constexpr float half_size = 2.0f;      // distance from house center to wall centerline
    static constexpr float wall_height = 2.0f;
    static constexpr float wall_thickness = 0.5f;
    static constexpr float roof_thickness = 0.25f;

    House(engine::IScene* my_scene, const glm::vec2& center, float ground_y);

    void on_start() override;
    void update(float dt) override;

    float get_roof_top_y() const { return ground_y_ + wall_height + roof_thickness; }
    glm::vec3 get_roof_center() const { return { center_.x, get_roof_top_y(), center_.y }; }
    // objects standing on the roof, hidden together with the roof
    void add_roof_prop(engine_game_object_t go) { roof_props_.push_back(go); }

private:
    bool is_inside(const glm::vec3& p) const;

private:
    glm::vec2 center_;
    float ground_y_ = 0.0f;
    bool player_inside_ = false;
    engine_game_object_t player_go_ = ENGINE_INVALID_GAME_OBJECT_ID;
    // child parts with a HideWhenInside other than eNever, found in on_start
    std::vector<BuildingPart*> hidden_when_inside_;
    std::vector<engine_game_object_t> roof_props_;
};

// A house centered on `center` (x, z) standing on ground_y, with its parts; parts whose prefab failed to load are skipped.
House* build_house(engine::IScene& scene, AppProjectC& app, const glm::vec2& center, float ground_y);

} // namespace project_c
