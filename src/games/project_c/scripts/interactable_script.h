#pragma once
#include <engine.h>
#include "base_script.h"

namespace project_c
{

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

protected:
    bool was_interacted_ = false;
};
}