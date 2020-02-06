/*
 * Simple Time-Series Database
 *
 * Timer
 *
 */

#pragma once

#include <chrono>

class Timer
{
private:
	typedef std::chrono::high_resolution_clock clock_t;
	typedef std::chrono::duration<float, std::ratio<1> > second_t;
	std::chrono::time_point<clock_t> m_Start;

public:
	Timer(void)
		: m_Start(clock_t::now()) {}
	~Timer(void) {}

	void Reset(void) { m_Start = clock_t::now(); }

	// returns elapsed time in seconds since creations or the last Reset
	float Elapsed(void) const
	{
		return std::chrono::duration_cast<second_t>(
			clock_t::now() - m_Start).count();
	}
};
