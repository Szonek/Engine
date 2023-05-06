#pragma once
#include "engine.h"

#include <vector>

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

    virtual engine_game_object_t get_game_object() const { return go_; }

protected:
    engine_application_t& app_;
    engine_scene_t& scene_;
    const engine_game_object_t go_;
};
