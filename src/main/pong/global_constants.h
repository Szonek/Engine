#pragma once
#include "engine.h"

#if __ANDROID__
constexpr const bool K_IS_ANDROID = true;
#else  
constexpr const bool K_IS_ANDROID = false;
#endif

constexpr const float K_WALL_Y_OFFSET = 8.0f;
constexpr const bool K_IS_GOAL_NET_DISABLE_RENDER = true;
