#include "player_input.h"

project_c::PlayerInput project_c::read_player_input_from_devices()
{
    PlayerInput input{};
    input.forward = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_W);
    input.back = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_S);
    input.left = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_A);
    input.right = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_D);
    input.attack = engineMouseIsButtonPressed(ENGINE_MOUSE_BUTTON_RIGHT);
    input.dodge = engineKeyboardIsButtonPressed(ENGINE_KEYBOARD_KEY_SPACE);
    input.interact = engineMouseIsButtonPressed(ENGINE_MOUSE_BUTTON_LEFT);
    input.drop_weapon = engineKeyboardIsButtonPressed(ENGINE_KEYBOARD_KEY_F);
    input.restart = engineKeyboardIsButtonPressed(ENGINE_KEYBOARD_KEY_R);
    // a press, not the held key: Escape also closes the editor's dialogs, and is still down when the game gets the
    // keyboard back
    input.quit = engineKeyboardIsButtonPressed(ENGINE_KEYBOARD_KEY_ESCAPE);
    input.debug_reset_position = engineKeyboardIsButtonPressed(ENGINE_KEYBOARD_KEY_0);
    input.cursor = engineMouseCoordsGet();
    return input;
}
