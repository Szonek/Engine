#pragma once
#include <engine.h>

namespace project_c
{
// What the player asks for this frame, in game terms. Gameplay scripts read it (AppProjectC::player_input()) instead of
// the keyboard and mouse, so the key bindings live in one place and the game can be driven without devices (tests).
// One-shot actions (dodge, drop weapon, restart, quit, debug reset) are true only in the frame their key was pressed,
// so a test sets them for one frame. The others are true while held; scripts detect an attack press on the up -> down
// change of `attack` (the mouse has no press of its own yet), so holding it for several frames is a single attack.
struct PlayerInput
{
    bool forward = false;               // W
    bool back = false;                  // S
    bool left = false;                  // A
    bool right = false;                 // D
    bool attack = false;                // right mouse button
    bool dodge = false;                 // space pressed (also lets go of a wall)
    bool interact = false;              // left mouse button
    bool drop_weapon = false;           // F pressed
    bool restart = false;               // R pressed (restarts the level once the encounter is over)
    bool quit = false;                  // Escape pressed this frame (handled by the main loop: closes the game)
    bool debug_reset_position = false;  // 0 pressed
    // same space as engineMouseCoordsGet: (0, 0) bottom-left of the window, (1, 1) top-right
    engine_fvec2_t cursor{ 0.5f, 0.5f };
};

// the key bindings: reads the real keyboard and mouse
PlayerInput read_player_input_from_devices();
}  // namespace project_c
