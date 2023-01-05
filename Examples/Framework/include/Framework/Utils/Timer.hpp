#pragma once

#include "Framework/Core/Common.hpp"

#include <chrono>

// Simple timer class to keep track of the elapsed time
class Timer
{
public:
	Timer()
	{
		reset();
	}

	// Re-captures starting time point to steady_clock::now
	void reset()
	{
		m_Start = std::chrono::steady_clock::now();
	}

	// Returns the elapsed time in seconds
	float elapsed_time()
	{
		const auto End = std::chrono::steady_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(End - m_Start)
		         .count()
		       / 1000.0f;
	}

private:
	std::chrono::time_point<std::chrono::steady_clock> m_Start;
};
