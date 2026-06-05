#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include "appState.hpp"

class TimelineManager
{
public:
    TimelineManager();
    ~TimelineManager();

    void run();
    void stop();

    // Reconstruct the timeline based on current parameters (preset, hour filters)
    static void updateTimeline(int presetHours, int startHour, int endHour);

private:
    void threadLoop();

private:
    std::atomic<bool> running{false};
    std::thread timelineThread;

    static std::mutex managerMutex;
};
