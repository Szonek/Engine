#include "game_timer.h"

engine::GameTimer::GameTimer()
    : _current_time(clock_type::now())
    , _stop_time(clock_type::now())
{
}

float engine::GameTimer::delta_time() const
{
    return static_cast<float>(_delta_time.count());
}

float engine::GameTimer::playing_time() const
{
    return static_cast<float>(std::chrono::duration_cast<timestamp_ms>(_current_time - _stop_time).count());
}

void engine::GameTimer::tick()
{
    _previous_time = _current_time;
    _current_time = clock_type::now();
    _delta_time = std::chrono::duration_cast<timestamp_ms>(_current_time - _previous_time);
}
