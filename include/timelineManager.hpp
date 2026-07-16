#pragma once
#include "appState.hpp"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class TimelineManager
{
  public:
	TimelineManager();
	~TimelineManager();

	void run();
	void stop();

	// Reconstruct the timeline based on current parameters (preset, hour
	// filters)
	static void updateTimeline(int presetHours, int startHour, int endHour);

  private:
	void threadLoop();

  private:
	std::atomic<bool> running{false};
	std::thread timelineThread;

	static std::mutex managerMutex;
};
