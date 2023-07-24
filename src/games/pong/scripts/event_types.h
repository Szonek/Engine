#pragma once
#include <cstdint>

namespace pong
{

enum PongEventType : std::uint32_t
{
    PONG_EVENT_TYPE_GOAL_SCORED = 0,
};

}  // namespace pong