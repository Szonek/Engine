#pragma once
#include "engine.h"
#include "utils.h"
#include <cstdint>
#include <vector>

namespace engine
{
class IScene;

class ENGINE_APP_TOOLKIT_API IScript
{
public:
    struct contact_point_t
    {
        engine_fvec3_t point_on_a;
        engine_fvec3_t point_on_b;
        std::int32_t lifetime;
    };
    struct collision_t
    {
        engine_game_object_t other;
        std::vector<contact_point_t> contact_points;
    };

public:
    // Lifecycle (see IScene): the constructor sets up only the script's own game object (its components, its own
    // prefab hierarchy); it never creates other game objects or looks up other scripts. Everything that involves other
    // objects happens in on_start().

    // take ownership of user-provided game object and associated scene
    IScript(IScene* my_scene, engine_game_object_t go);
    // creates new game object in given scene, part of the scene's content (not transient)
    IScript(IScene* my_scene);
    virtual ~IScript();

    // Called once, before the first update, when every script registered in the same frame exists (so they can find
    // each other) and gameplay is playing. Look up other scripts and spawn runtime objects here; objects spawned at
    // runtime are transient (engineGameObjectCreateTransient / enginePrefabInstantiateTransient).
    virtual void on_start() {}
    virtual void update(float dt) {} // use it for main game logic
    virtual void late_update(float dt) {} // use it for game logic which depends on main logic (i.e. transforms, camera followign a character etc.)
    //ToDo: this should be moved to seperate class like  ICollidableScript
    virtual void on_collision(const collision_t& info) {}
    

    virtual engine_game_object_t get_game_object() const { return go_; }

protected:
    // False once the game object is gone. After Stop the scene was cleared and loaded back before the scripts of the play
    // session are dropped (IScene::end_play_session), so their destructors run after their game objects (and the runtime
    // objects they spawned) were destroyed: a destructor which changes its game object or the scene checks this first.
    bool has_game_object() const { return engineGameObjectIsValid(go_); }

    IScene* my_scene_ = nullptr;
    const engine_game_object_t go_;

private:
    friend class IScene;
    // false when the scene removes the script but keeps its game object (its script component was removed or changed)
    bool destroys_game_object_ = true;
};

} // namespace engine