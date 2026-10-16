#pragma once
#include "base_script.h"

#include <cstdint>

namespace project_c
{
// The rules of a fight: cleared once every enemy in the scene is dead, defeated once the player is.
// It reads the state of the Player and Enemy scripts each frame; nothing reports to it. Scripts that care
// find it with IScene::find_script<Encounter>(); a scene without one has no win/lose rules.
class Encounter : public BaseNode
{
public:
    enum class State
    {
        eInProgress,
        eCleared,
        eDefeated
    };

    Encounter(engine::IScene* my_scene);

    void update(float dt) override;

    State get_state() const { return state_; }
    bool is_over() const { return state_ != State::eInProgress; }
    // alive enemies as of the last update
    std::uint32_t get_remaining_enemies() const { return remaining_enemies_; }

private:
    State state_ = State::eInProgress;
    std::uint32_t remaining_enemies_ = 0;
    // a scene that never had enemies is never cleared
    bool had_enemies_ = false;
};
}  // namespace project_c
