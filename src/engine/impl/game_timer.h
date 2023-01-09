#pragma once

#include <chrono>

namespace engine
{
class GameTimer
{
public:
    GameTimer();

	float delta_time() const;
	float playing_time() const;
	void tick();

private:
	using clock_type = std::chrono::high_resolution_clock;
	using timestamp_ms = std::chrono::milliseconds;

	clock_type::time_point _current_time;
	clock_type::time_point _previous_time;
	timestamp_ms _delta_time;

	clock_type::time_point _stop_time;
};
}  // namespace engine