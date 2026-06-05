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

static bool isInvalidApp(const std::string& appName) {
    if (appName.empty()) return true;
    std::string lower = appName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "unknown" || lower == "unknow") return true;
    return false;
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
        AppState::timelineMarkers.clear();
        return;
    }

    // 2. Flatten transitions (keep all transitions as boundaries)
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


    if (transitions.empty())
    {
        std::lock_guard<std::mutex> lock(AppState::timelineMutex);
        AppState::timelineEvents.clear();
        AppState::timelineMarkers.clear();
        return;
    }

    // 3. Sort chronologically
    std::sort(transitions.begin(), transitions.end(), [](const Transition& a, const Transition& b) {
        return a.ts < b.ts;
    });


    // 4. Calculate time range boundaries
    // Get start/end based on current day boundaries
    std::time_t tt_first = static_cast<std::time_t>(transitions[0].ts / 1000);
    std::tm tmStart = *std::localtime(&tt_first);
    tmStart.tm_hour = 0; tmStart.tm_min = 0; tmStart.tm_sec = 0;
    uint64_t dayStartMs = static_cast<uint64_t>(std::mktime(&tmStart)) * 1000ULL;

    uint64_t T_start = dayStartMs + (startHour * 3600ULL * 1000ULL);
    uint64_t T_end = dayStartMs + (endHour * 3600ULL * 1000ULL);

    uint64_t lastEnd = transitions.back().ts + 600000; // Historical default active end
    if (currentView == AppState::CurrentView::LIVE)
    {
        lastEnd = nowMs;
    }

    uint64_t totalSpan = (T_end > T_start) ? (T_end - T_start) : 1;

    // Generate dynamic hourly markers
    std::vector<AppState::TimelineMarkerInternal> markers;
    int hourStep = (presetHours == 24 || presetHours == 0) ? 3 : 1;
    for (int h = startHour; h <= endHour; h += hourStep)
    {
        uint64_t ts = dayStartMs + (h * 3600ULL * 1000ULL);
        double x = static_cast<double>(ts - T_start) / static_cast<double>(totalSpan);

        int displayHour = h % 12;
        if (displayHour == 0) displayHour = 12;
        std::string suffix = " AM";
        if (h >= 12 && h < 24) suffix = " PM";
        std::string label = (displayHour < 10 ? "0" : "") + std::to_string(displayHour) + ":00" + suffix;

        markers.push_back({x, label});
    }

    // 5. Reconstruct and filter segments with fractional boundaries
    std::vector<AppState::TimelineEventInternal> filteredSegments;
    
    // First transition segment
    uint64_t firstStart = (transitions[0].ts > 600000) ? (transitions[0].ts - 600000) : 0;
    
    auto processSegment = [&](const std::string& appName, uint64_t segStart, uint64_t segEnd) {
        if (isInvalidApp(appName))
        {
            return;
        }

        // Check if segment overlaps viewport range
        if (segEnd <= T_start || segStart >= T_end)
        {
            return;
        }

        // Clamp to viewport boundaries
        uint64_t clampedStart = (std::max)(segStart, T_start);
        uint64_t clampedEnd = (std::min)(segEnd, T_end);

        if (clampedEnd <= clampedStart) return;

        double x = static_cast<double>(clampedStart - T_start) / static_cast<double>(totalSpan);
        double width = static_cast<double>(clampedEnd - clampedStart) / static_cast<double>(totalSpan);

        std::string aliasedName = AppState::aliasManager.getAlias(appName);
        std::string rangeStr = formatTime_HHMM(segStart) + " - " + formatTime_HHMM(segEnd);
        std::string durationStr = formatTime_HHMMSS(segEnd - segStart);

        filteredSegments.push_back({
            x,
            width,
            aliasedName,
            durationStr,
            rangeStr
        });
    };

    // Process Segment 0
    processSegment(transitions[0].fromApp, firstStart, transitions[0].ts);

    // Process Middle segments
    for (size_t i = 0; i < transitions.size() - 1; ++i)
    {
        uint64_t segStart = transitions[i].ts;
        uint64_t segEnd = transitions[i+1].ts;
        if (isInvalidApp(transitions[i+1].fromApp))
        {
            segEnd = std::min<uint64_t>(segEnd, segStart + 60000ULL);
        }
        processSegment(transitions[i].toApp, segStart, segEnd);
    }

    // Process Last segment
    processSegment(transitions.back().toApp, transitions.back().ts, lastEnd);

    // 6. Thread-safely update cache
    {
        std::lock_guard<std::mutex> lock(AppState::timelineMutex);
        AppState::timelineEvents = std::move(filteredSegments);
        AppState::timelineMarkers = std::move(markers);
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
