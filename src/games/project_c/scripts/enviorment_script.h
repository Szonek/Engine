#pragma
#include "base_script.h"

namespace project_c
{

class DebugPathNode : public BaseNode
{
public:
    DebugPathNode(engine::IScene* my_scene, float offset_x, float offset_z);
};

class EnviormentBaseScript : public BaseNode
{
protected:
    EnviormentBaseScript(engine::IScene* my_scene, engine_game_object_t go, std::string_view name);
};

class Floor : public EnviormentBaseScript
{
public:
    Floor(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_z);
};

class Wall : public EnviormentBaseScript
{
public:
    Wall(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_z);
};

class Barrel : public BaseNode
{
public:
    Barrel(engine::IScene* my_scene, engine_game_object_t go);
};

class Interactable
{
public:
    virtual ~Interactable() = default;
    virtual void interact() = 0;
};

class Chest : public BaseNode, public Interactable
{
public:
    Chest(engine::IScene* my_scene, engine_game_object_t go);
    void interact() override;
};


class LightBaseScript : public BaseNode
{
public:
    LightBaseScript(engine::IScene* my_scene, std::string_view name);
};

class MainLight : public LightBaseScript
{
public:
    MainLight(engine::IScene* my_scene);
};

class PointLight : public LightBaseScript
{
public:
    PointLight(engine::IScene* my_scene);
};

class SpotLight : public LightBaseScript
{
public:
    SpotLight(engine::IScene* my_scene);
};

} //namespace project_c