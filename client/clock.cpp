#include "client/clock.hpp"

#include "client/platform/platform.hpp"

void Clock::reset()
{
	start_time = platform_get_absolute_time();
}

double Clock::get_elapsed_time() const
{
	return platform_get_absolute_time() - start_time;
}
