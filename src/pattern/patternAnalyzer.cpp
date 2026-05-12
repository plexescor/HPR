#include "patternAnalyzer.hpp"
#include "appState.hpp"
#include "aliasManager.hpp"
#include "timeUtils.hpp"

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <mutex>
#include <algorithm>
#include <iostream>

void PatternAnalyzer::generateInsights()
{
    //Make a copy of appstate's map into this
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        timeLog_PerApp = AppState::state.timeLog_PerApp;
        switchHistory = AppState::state.switchHistory;
    }

    //PATTERN 1: MOST USED APP
    if (!timeLog_PerApp.empty()) 
    {
        auto maxIt = std::max_element(timeLog_PerApp.begin(), timeLog_PerApp.end(),
        [](const auto& a, const auto& b) 
        {
            return a.second < b.second;
        });
        std::string name = AppState::aliasManager.getAlias(maxIt->first);
        std::string time = formatTime_HHMMSS(maxIt->second);

        mostUsed_O = name + " — " + time; //i intentionally used —, no ai

        // std::cout << mostUsed_O << std::endl;
    }

    //PATTERN 2: Total tracked time
    if (!timeLog_PerApp.empty())
    {
        int totalTrackedTime = 0;
        for (const auto &[raw, duration] : timeLog_PerApp)
        {
            totalTrackedTime += duration;
        }

        std::string time = formatTime_HHMMSS(totalTrackedTime);
        totalTrackedTime_O = time;

        // std::cout << totalTrackedTime_O << std::endl;
    }

    //PATTERN 3: Total app switches
    if (!switchHistory.empty())
    {
        size_t total = 0;
        for (const auto& [key, vec] : switchHistory) 
        {
            total += vec.size();
        }
        switchCount_O = std::to_string(total);

        // std::cout << switchCount_O << std::endl;
    }

    //PATTERN 4: Most Switched-Away From App
    if (!switchHistory.empty())
    {
        std::map<std::string, size_t> switchCounts;
        const std::string selfApp = "HPR";
        for (const auto& [apps, vec] : switchHistory)
        {
            const std::string& fromApp = apps.first;
            if (fromApp == selfApp) continue;
            switchCounts[fromApp] += vec.size();
        }
        auto maxIt = std::max_element(
            switchCounts.begin(),
            switchCounts.end(),
            [](const auto& a, const auto& b)
            {
                return a.second < b.second;
            });

        std::string app = AppState::aliasManager.getAlias(maxIt->first);    
        size_t count = maxIt->second;

        mostSwitchedFrom_O = app + " — " + std::to_string(count) + " switches";
    }

    //PATTERN 5: Most Switched-To From App
    if (!switchHistory.empty())
    {
        std::map<std::string, size_t> switchCounts;
        const std::string selfApp = "HPR";

        for (const auto& [apps, vec] : switchHistory)
        {
            const std::string& toApp = apps.second;
            if (toApp == selfApp) continue; // skip switches back to HPR
            switchCounts[toApp] += vec.size();
        }

        auto maxIt = std::max_element(
            switchCounts.begin(),
            switchCounts.end(),
            [](const auto& a, const auto& b)
            {
                return a.second < b.second;
            });

        std::string app = AppState::aliasManager.getAlias(maxIt->first);    
        size_t count = maxIt->second;

        mostSwitchedTo_O = app + " — " + std::to_string(count) + " switches";
    }

    //Pattern 6: Longest Focus Session 
    if (!switchHistory.empty())
    {
        // For each app, collect: (arrived_at_ts, left_at_ts) pairs  
        // Build per-app: list of departure timestamps (when we LEFT that app)
        std::map<std::string, std::vector<uint64_t>> departures;
        std::map<std::string, std::vector<uint64_t>> arrivals;
        const std::string selfApp = "HPR";
        for (const auto& [apps, vec] : switchHistory)
        {   
            if (apps.second == selfApp) continue;
            // std::cout << "FROM: [" << apps.first << "] TO: [" << apps.second << "]\n";
            for (uint64_t ts : vec)
            {
                departures[apps.first].push_back(ts);
                arrivals[apps.second].push_back(ts);
            }
        }

        for (auto& [app, vec] : departures) std::sort(vec.begin(), vec.end());
        for (auto& [app, vec] : arrivals)   std::sort(vec.begin(), vec.end());

        uint64_t bestDuration = 0;
        std::string bestApp;

        const uint64_t maxGap_ms = 8ULL * 60 * 60 * 1000;

        for (auto& [app, depTimes] : departures)
        {
            auto arrIt = arrivals.find(app);
            if (arrIt == arrivals.end()) continue;

            const auto& arrTimes = arrIt->second;

            // For each departure, find the most recent arrival before it
            for (uint64_t dep : depTimes)
            {
                // last arrival strictly before dep
                //i didnt wanna usr raw ptr :)
                auto it = std::lower_bound(arrTimes.begin(), arrTimes.end(), dep);
                while (it != arrTimes.begin()) {
                    --it;
                    if (*it < dep) break; // found strictly earlier arrival
                    // if equal, keep going back
                }
                if (*it >= dep) continue; // no valid arrival found

                uint64_t arr = *it;
                uint64_t duration = dep - arr;

                if (duration >= maxGap_ms) continue; // cross-session
                if (duration < 1000) continue; // ignore sub-1s noise , same-event timestamp collisions

                if (duration > bestDuration)
                {
                    bestDuration = duration;
                    bestApp = app;
                }
            }
        }


        if (!bestApp.empty())
        {
            std::string app = AppState::aliasManager.getAlias(bestApp);
            std::string time = formatTime_HHMMSS(static_cast<int>(bestDuration));
            mostFocusedSession_O = time + " — " + app;
        }
    }

    //Pattern 7: Peak Productive Hour
    if (!switchHistory.empty())
    {
        std::vector<uint64_t> timestamps;

        // flatten all timestamps into one vector
        for (const auto& [key, vec] : switchHistory)
        {
            timestamps.insert(
                timestamps.end(),
                vec.begin(),
                vec.end()
            );
        }

        std::sort(timestamps.begin(), timestamps.end());

        // window must be between 60 and 90 mins — epochs are milliseconds
        const uint64_t minWindow_ms = 60ULL * 60 * 1000;
        const uint64_t maxWindow_ms = 90ULL * 60 * 1000;

        uint64_t bestStart = 0;
        uint64_t bestEnd = 0;

        size_t left = 0;
        size_t bestCount = SIZE_MAX;
        bool foundValid = false;

        // sliding window — fewer switches in window = more focused
        for (size_t right = 0; right < timestamps.size(); ++right)
        {
            // shrink window from left if it exceeds 90 mins
            while (timestamps[right] - timestamps[left] > maxWindow_ms)
            {
                ++left;
            }

            uint64_t windowSpan = timestamps[right] - timestamps[left];

            // only consider windows that are at least 60 mins wide
            if (windowSpan < minWindow_ms) continue;

            size_t count = right - left + 1;

            // fewer switches = more focused/ productive
            if (count < bestCount)
            {
                bestCount = count;
                bestStart = timestamps[left];
                bestEnd = timestamps[right];
                foundValid = true;
            }
        }

        if (!foundValid)
        {
            mostProductiveHour_O = "Not enough data";
        }
        else
        {
            std::string start = convertToTime_HHMMSS_12(bestStart);
            std::string end = convertToTime_HHMMSS_12(bestEnd);

            // trying this inline shit for first time
            auto formatTime = [](std::string t)
            {
                std::replace(t.begin(), t.end(), '-', ':');

                // remove seconds part
                size_t secondColon = t.find(':', t.find(':') + 1);
                if (secondColon != std::string::npos)
                {
                    size_t spacePos = t.find(' ', secondColon);

                    t.erase(secondColon, spacePos - secondColon);
                    t.erase(std::remove(t.begin(), t.end(), ' '), t.end());
                }

                return t;
            };

            mostProductiveHour_O = formatTime(start) + " — " + formatTime(end);
        }
    }
}

std::string PatternAnalyzer::getMostUsed()
{
    return mostUsed_O;
}

std::string PatternAnalyzer::getTotalTrackedTime()
{
    return totalTrackedTime_O;
}

std::string PatternAnalyzer::getSwitchCount()
{
    return switchCount_O;
}

std::string PatternAnalyzer::getMostSwitchedFrom()
{
    return mostSwitchedFrom_O;
}

std::string PatternAnalyzer::getMostSwitchedTo()
{
    return mostSwitchedTo_O;
}

std::string PatternAnalyzer::getMostFocusedSession()
{
    return mostFocusedSession_O;
}

std::string PatternAnalyzer::getMostProductiveHour()
{
    return mostProductiveHour_O;
}