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
		start = std::chrono::steady_clock::now();
	}

	// Returns the elapsed time in seconds
	float elapsed_time()
	{
		const auto end = std::chrono::steady_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0f;
	}

private:
	std::chrono::time_point<std::chrono::steady_clock> start;
};
