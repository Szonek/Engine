#pragma once
#include <cstdint>

namespace project_c
{
// what the player holds; a new kind of weapon gets its own value
enum class WeaponType : std::uint8_t
{
    eNone,
    eDaggers,
};

// The player's persistent stats: what outlives a level (like Unreal's GameInstance data or a Godot autoload). Runtime
// state, never in scene files. There is one, owned by AppProjectC: the Player script reads and writes it there and
// keeps no copy, so a new stat only needs a field here (and its fresh value in Player::fresh_state).
struct PlayerState
{
    std::uint32_t hp = 0;
    std::uint64_t coins = 0;
    WeaponType weapon = WeaponType::eNone;
};
}  // namespace project_c
