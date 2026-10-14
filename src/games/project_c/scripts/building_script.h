#pragma once
#include "base_script.h"
#include "camera_script.h"

#include <optional>
#include <vector>

#include <glm/glm.hpp>

namespace project_c
{
class AppProjectC;

// Static piece of a building: a visible piece (mesh, no collider) or an invisible collision block.
// Player traversal (climb/vault/mantle) only reacts to colliders owned by BuildingParts.
// Gets out of the camera's way when it hides the player (see OcclusionResponse).
class BuildingPart : public BaseNode, public CameraOccluder
{
public:
    enum class OcclusionResponse
    {
        eNone,
        eLower,     // sink the mesh down to the player's feet ("walls down"), needs a part without collider
        eHide       // hide the mesh
    };

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

    void update(float dt) override;

    // surface_normal - normal of the face player wants to climb
    bool is_climbable(const glm::vec3& surface_normal) const;
    // Only faces pointing away from this point are climbable, i.e. house walls can be climbed only from the outside.
    void set_climbable_away_from(const glm::vec3& point) { climbable_away_from_ = point; }

    // Point traversal should align to, i.e. center of the window opening for window frame parts.
    void set_alignment_point(const glm::vec3& point) { alignment_point_ = point; }
    std::optional<glm::vec3> get_alignment_point() const { return alignment_point_; }

    // Hides mesh, and optionally removes the collider (so mouse picking goes through hidden roofs).
    void set_hidden(bool hidden, bool affect_collider);

    // full_height - world height of the mesh, eLower only
    void set_occlusion_response(OcclusionResponse response, float full_height = 0.0f);
    // Invisible colliders forward occlusion to the visible piece they belong to.
    void set_occlusion_owner(engine_game_object_t go) { occlusion_owner_ = go; }
    // Shown and hidden together with this part, i.e. props standing on the roof.
    void add_linked_prop(engine_game_object_t go);

    engine_game_object_t get_occlusion_owner() const override { return occlusion_owner_ != ENGINE_INVALID_GAME_OBJECT_ID ? occlusion_owner_ : go_; }
    void set_occluding(bool occluding, float player_feet_y) override;

private:
    void add_collider();
    // mesh visibility (and linked props) from hidden_ + occluding_
    void apply_visibility();

private:
    std::optional<Box> collider_;
    std::optional<glm::vec3> alignment_point_;
    std::optional<glm::vec3> climbable_away_from_;
    glm::vec3 position_{ 0.0f };
    glm::vec3 scale_{ 1.0f };
    engine_game_object_t occlusion_owner_ = ENGINE_INVALID_GAME_OBJECT_ID;
    std::vector<engine_game_object_t> linked_props_;
    OcclusionResponse occlusion_response_ = OcclusionResponse::eNone;
    float full_height_ = 0.0f;
    float height_fraction_ = 1.0f;          // eLower: current visible part of the mesh height
    float target_height_fraction_ = 1.0f;
    bool climbable_ = false;
    bool hidden_ = false;
    bool occluding_ = false;
    bool collider_removed_ = false;
};

// Small, single room house: 4 walls (one with an open window), walkable roof.
// While the player is inside, the roof is hidden and the wall facing the camera is lowered.
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
    void add_roof_prop(engine_game_object_t go);

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
    std::vector<BuildingPart*> camera_facing_walls_;
    CameraScript* camera_ = nullptr;
    BuildingPart* roof_ = nullptr;
};

} // namespace project_c
