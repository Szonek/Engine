#pragma once

#include <chrono>

namespace engine
{
class GameTimer
{
public:
    using timestamp_type = std::chrono::microseconds;
public:
    GameTimer();

    timestamp_type delta_time() const;
    timestamp_type playing_time() const;
	void tick();

private:
	using clock_type = std::chrono::high_resolution_clock;


	clock_type::time_point _current_time;
	clock_type::time_point _previous_time;
	timestamp_type _delta_time;

	clock_type::time_point _stop_time;
};
}  // namespace engine