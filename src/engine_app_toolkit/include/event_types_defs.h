#pragma once
#include "engine.h"

namespace engine
{

struct PointerEventData
{
    engine_game_object_t pointer_click_object = ENGINE_INVALID_GAME_OBJECT_ID;
    engine_game_object_t pointer_down_object = ENGINE_INVALID_GAME_OBJECT_ID;
    engine_mouse_button_t button = {};
    float position[2] = {};
};

}