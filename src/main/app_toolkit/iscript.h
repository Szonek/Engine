#pragma once
#include "engine.h"

#include <vector>

namespace engine
{

class IScript
{
public:
    struct contact_point_t
    {
        float point[3];
        std::int32_t lifetime;
    };
    struct collision_t
    {
        engine_game_object_t other;
        std::vector<contact_point_t> contact_points;
    };

public:
    IScript(engine_application_t& app, engine_scene_t& scene)
        : app_(app)
        , scene_(scene)
        , go_(engineSceneCreateGameObject(scene))
    {}
    virtual ~IScript() = default;

    virtual void update(float dt) {}
    virtual void on_collision(const collision_t& info) {}

    // if the button/touch is pressed and released on the same object 
    virtual void on_pointer_click(const struct PointerEventData* ped) {};
    virtual void on_pointer_down(const struct PointerEventData* ped) {};

    virtual engine_game_object_t get_game_object() const { return go_; }

protected:
    engine_application_t& app_;
    engine_scene_t& scene_;
    const engine_game_object_t go_;
};

} // namespace engine