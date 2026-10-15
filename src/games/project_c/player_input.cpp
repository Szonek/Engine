#include "player_input.h"

project_c::PlayerInput project_c::read_player_input_from_devices()
{
    PlayerInput input{};
    input.forward = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_W);
    input.back = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_S);
    input.left = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_A);
    input.right = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_D);
    input.attack = engineMouseIsButtonDown(ENGINE_MOUSE_BUTTON_RIGHT);
    input.dodge = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_SPACE);
    input.interact = engineMouseIsButtonDown(ENGINE_MOUSE_BUTTON_LEFT);
    input.drop_weapon = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_F);
    input.debug_reset_position = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_0);
    input.cursor = engineMouseCoordsGet();
    return input;
}
