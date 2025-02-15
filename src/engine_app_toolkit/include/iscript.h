#pragma once
#include "engine.h"
#include "utils.h"
#include <vector>

namespace engine
{
class IScene;

class ENGINE_APP_TOOLKIT_API IScript
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
    // take ownership of user-provided game object and associated scene 
    IScript(IScene* my_scene, engine_game_object_t go);
    // creates new game object in given scene
    IScript(IScene* my_scene);
    virtual ~IScript();

    virtual void update(float dt) {}
    //ToDo: this should be moved to seperate class like  ICollidableScript
    virtual void on_collision(const collision_t& info) {}
    

    virtual engine_game_object_t get_game_object() const { return go_; }

protected:
    IScene* my_scene_ = nullptr;
    const engine_game_object_t go_;
};

} // namespace engine