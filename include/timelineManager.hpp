#pragma once
#include "appState.hpp"
#include "appEvents.hpp"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <condition_variable>

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
	std::atomic<bool> paused{false};
	std::mutex pauseMutex;
	std::condition_variable pauseCv;

	size_t uiVisibleId;
	size_t uiHiddenId;

	static std::mutex managerMutex;
};
