#include "timelineManager.hpp"
#include "appState.hpp"
#include "timeUtils.hpp"
#include "aliasManager.hpp"
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <iostream>

std::mutex TimelineManager::managerMutex;

TimelineManager::TimelineManager()
{
}

TimelineManager::~TimelineManager()
{
    stop();
}

void TimelineManager::run()
{
    running = true;
    timelineThread = std::thread(&TimelineManager::threadLoop, this);
}

void TimelineManager::stop()
{
    running = false;
    if (timelineThread.joinable())
    {
        timelineThread.join();
    }
}

// Format epoch milliseconds to "HH:MM AM/PM"
static std::string formatTime_HHMM(uint64_t ms)
{
    try
    {
        std::time_t tt = static_cast<std::time_t>(ms / 1000);
        std::tm tm = *std::localtime(&tt);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%I:%M %p");
        return oss.str();
    }
    catch (...)
    {
        return "";
    }
}

void TimelineManager::updateTimeline(int presetHours, int startHour, int endHour)
{
    std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;
    AppState::CurrentView currentView;
    uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // 1. Safely copy switch history and view mode
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        currentView = AppState::state.currentView;

        if (currentView == AppState::CurrentView::LIVE)
        {
            switchHistory = AppState::state.switchHistory;
        }
        else if (currentView == AppState::CurrentView::HISTORICAL_SINGULAR)
        {
            std::lock_guard<std::mutex> histLock(AppState::historyStateMutex);
            switchHistory = AppState::historicalData_State.switchHistory;
        }
        else
        {
            std::lock_guard<std::mutex> histLock(AppState::historyStateMutex);
            switchHistory = AppState::historicalData_Full_State.switchHistory;
        }
    }

    if (switchHistory.empty())
    {
        std::lock_guard<std::mutex> lock(AppState::timelineMutex);
        AppState::timelineEvents.clear();
        return;
    }

    // 2. Flatten transitions
    struct Transition {
        std::string fromApp;
        std::string toApp;
        uint64_t ts;
    };
    std::vector<Transition> transitions;
    for (const auto& [route, timestamps] : switchHistory)
    {
        for (uint64_t ts : timestamps)
        {
            transitions.push_back({route.first, route.second, ts});
        }
    }

    // 3. Sort chronologically
    std::sort(transitions.begin(), transitions.end(), [](const Transition& a, const Transition& b) {
        return a.ts < b.ts;
    });

    // 4. Reconstruct timeline segments
    std::vector<AppState::TimelineEventInternal> rawSegments;
    if (transitions.empty()) return;

    // First transition: transitions[0].fromApp active before transitions[0].ts
    // We assume it was active for a short default (e.g., 10 minutes) before the first switch
    uint64_t firstStart = (transitions[0].ts > 600000) ? (transitions[0].ts - 600000) : 0;
    rawSegments.push_back({
        transitions[0].fromApp,
        formatTime_HHMM(firstStart),
        formatTime_HHMM(transitions[0].ts),
        formatTime_HHMMSS(transitions[0].ts - firstStart),
        static_cast<double>(transitions[0].ts - firstStart)
    });

    // Middle segments
    for (size_t i = 0; i < transitions.size() - 1; ++i)
    {
        uint64_t start = transitions[i].ts;
        uint64_t end = transitions[i+1].ts;
        uint64_t duration = (end > start) ? (end - start) : 0;

        rawSegments.push_back({
            transitions[i].toApp,
            formatTime_HHMM(start),
            formatTime_HHMM(end),
            formatTime_HHMMSS(duration),
            static_cast<double>(duration)
        });
    }

    // Last segment
    uint64_t lastStart = transitions.back().ts;
    uint64_t lastEnd = nowMs;
    if (currentView != AppState::CurrentView::LIVE)
    {
        // For historical data, assume active for 10 minutes or until end of that calendar day
        lastEnd = lastStart + 600000;
    }
    uint64_t lastDuration = (lastEnd > lastStart) ? (lastEnd - lastStart) : 0;
    rawSegments.push_back({
        transitions.back().toApp,
        formatTime_HHMM(lastStart),
        formatTime_HHMM(lastEnd),
        formatTime_HHMMSS(lastDuration),
        static_cast<double>(lastDuration)
    });

    // 5. Apply range and hour zoom filters
    // Calculate reference time boundary for presets
    uint64_t filterStartPresetMs = 0;
    if (presetHours > 0)
    {
        uint64_t refTime = (currentView == AppState::CurrentView::LIVE) ? nowMs : lastEnd;
        filterStartPresetMs = (refTime > (presetHours * 3600ULL * 1000ULL)) ? (refTime - (presetHours * 3600ULL * 1000ULL)) : 0;
    }

    // Calculate reference time boundary for custom hour zoom
    // We obtain the day start of the first transition
    std::time_t tt = static_cast<std::time_t>(transitions[0].ts / 1000);
    std::tm tmStart = *std::localtime(&tt);
    tmStart.tm_hour = 0; tmStart.tm_min = 0; tmStart.tm_sec = 0;
    uint64_t dayStartMs = static_cast<uint64_t>(std::mktime(&tmStart)) * 1000ULL;

    uint64_t zoomStartMs = dayStartMs + (startHour * 3600ULL * 1000ULL);
    uint64_t zoomEndMs = dayStartMs + (endHour * 3600ULL * 1000ULL);

    std::vector<AppState::TimelineEventInternal> filteredSegments;
    for (size_t i = 0; i < rawSegments.size(); ++i)
    {
        // Calculate raw segment epoch timestamps
        uint64_t segStart = (i == 0) ? firstStart : transitions[i-1].ts;
        uint64_t segEnd = (i == rawSegments.size() - 1) ? lastEnd : transitions[i].ts;

        // Apply Preset Filter
        if (presetHours > 0 && segEnd < filterStartPresetMs)
        {
            continue;
        }

        // Apply Zoom hour filter (must overlap with the selected start/end range)
        if (segEnd < zoomStartMs || segStart > zoomEndMs)
        {
            continue;
        }

        // Apply Alias mapping
        std::string aliasedAppName = AppState::aliasManager.getAlias(rawSegments[i].appName);

        filteredSegments.push_back({
            aliasedAppName,
            rawSegments[i].startTime,
            rawSegments[i].endTime,
            rawSegments[i].duration,
            rawSegments[i].durationMs
        });
    }

    // 6. Thread-safely update cache
    {
        std::lock_guard<std::mutex> lock(AppState::timelineMutex);
        AppState::timelineEvents = std::move(filteredSegments);
    }
}

void TimelineManager::threadLoop()
{
    while (running)
    {
        // Simple periodic update loop
        // Standard presets are updated based on cached parameters, which we'll fetch from the active UI properties later.
        // For now, we will perform a regular rebuild of the timeline.
        // Parameters will be updated directly by UiModelManager reading from the UI properties.
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}
