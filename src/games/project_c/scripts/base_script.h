#pragma once
#include <iscript.h>
#include <profiler.h>
#include "../prefab_types.h"
namespace project_c
{
class BaseNode : public engine::IScript
{
public:
    virtual void set_world_position(float x, float y, float z);

protected:
    // Like every script, it sets up nothing saved: what its game object is made of (name, transform, components) is
    // level content, placed by the level (LevelBuilder) or loaded from a scene file.
    BaseNode(engine::IScene* my_scene, engine_game_object_t go);
    // a runtime object (transient, never saved), which its script names
    BaseNode(engine::IScene* my_scene, engine_game_object_t go, std::string_view name);
    virtual ~BaseNode() = default;
};
} // namespace project_c