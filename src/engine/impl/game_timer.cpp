#include "game_timer.h"

engine::GameTimer::GameTimer()
    : _current_time(clock_type::now())
    , _stop_time(clock_type::now())
{
}

engine::GameTimer::timestamp_type engine::GameTimer::delta_time() const
{
    return _delta_time;
}

engine::GameTimer::timestamp_type engine::GameTimer::playing_time() const
{
    return std::chrono::duration_cast<timestamp_type>(_current_time - _stop_time);
}

void engine::GameTimer::tick()
{
    _previous_time = _current_time;
    _current_time = clock_type::now();
    _delta_time = std::chrono::duration_cast<timestamp_type>(_current_time - _previous_time);
}
