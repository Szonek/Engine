#pragma once
#include "base_script.h"

namespace project_c
{
// A marker of where an enemy's path goes (debug drawing), a runtime object.
class DebugPathNode : public BaseNode
{
public:
    DebugPathNode(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_z);
};

// A floor tile. It has no behaviour: other scripts recognize the floor by it (a dropped weapon lands on it).
class Floor : public BaseNode
{
public:
    Floor(engine::IScene* my_scene, engine_game_object_t go);
};
} //namespace project_c
