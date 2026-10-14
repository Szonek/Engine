#pragma once
#include <engine.h>
#include "base_script.h"
#include "camera_script.h"

namespace project_c
{

class Interactable
{
public:
    virtual ~Interactable() = default;
    virtual void interact() = 0;
};

// Fades when it hides the player (i.e. chest on a roof, player behind the house).
class Chest : public BaseNode, public Interactable, public CameraOccluder
{
public:
    Chest(engine::IScene* my_scene, engine_game_object_t go);
    void interact() override;
    void get_fade_targets(std::vector<engine_game_object_t>& out) const override { out.push_back(go_); }

protected:
    bool was_interacted_ = false;
};
}