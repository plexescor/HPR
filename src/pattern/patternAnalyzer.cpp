#include "patternAnalyzer.hpp"
#include "aliasManager.hpp"
#include "appState.hpp"
#include "logger.hpp"
#include "timeUtils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <mutex>
#include <filesystem>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

// Given a day's switchHistory, reconstruct all focus sessions longer than
// minMs. Returns a vector of {duration_ms, start_ts_ms} pairs.
struct Session
{
	uint64_t duration;
	uint64_t startTs;
};
std::vector<Session> buildSessions(const std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> &sh,
								   uint64_t minMs = 30000ULL)
{
	struct Ev
	{
		uint64_t ts;
		std::string app;
		bool arrival;
	};
	std::vector<Ev> events;

	auto isSelf = [](const std::string &n) { return n == "HPR" || n == "Unknown" || n.empty(); };

	for (const auto &[apps, vec] : sh)
	{
		for (uint64_t ts : vec)
		{
			if (!isSelf(apps.first))
				events.push_back({ts, apps.first, false});
			if (!isSelf(apps.second))
				events.push_back({ts, apps.second, true});
		}
	}

	std::sort(events.begin(), events.end(),
			  [](const Ev &a, const Ev &b) { return a.ts != b.ts ? a.ts < b.ts : (int)a.arrival < (int)b.arrival; });

	std::map<std::string, uint64_t> active; // app → arrival ts
	std::vector<Session> sessions;

	for (const auto &e : events)
	{
		if (e.arrival)
		{
			if (active.count(e.app))
			{
				active.erase(e.app); // Discard old arrival that had no departure
			}
			active[e.app] = e.ts;
		}
		else
		{
			if (active.count(e.app))
			{
				uint64_t dur = (e.ts >= active[e.app]) ? (e.ts - active[e.app]) : 0;
				if (dur >= minMs && dur < 8ULL * 3600 * 1000)
					sessions.push_back({dur, active[e.app]});
				active.erase(e.app);
			}
		}
	}
	return sessions;
}

PatternAnalyzer::PatternAnalyzer()
{
	initialiseCategoryFilePath();
	initialiseCategories();
}

void PatternAnalyzer::initialiseCategoryFilePath()
{
	std::filesystem::path tempPath;
#ifdef _WIN32
	tempPath = std::getenv("APPDATA");
	tempPath /= std::filesystem::path("HPR/HPR_Config/");
#else
	const char *home = std::getenv("HOME");
	if (!home)
		throw std::runtime_error("HOME env var not set");
	tempPath = home;
	tempPath /= std::filesystem::path(".config/HPR/");
#endif

	std::filesystem::create_directories(tempPath);
	filePath = tempPath / fileName;

	std::ifstream file(filePath);

	if (!file.is_open())
	{
		std::cerr << "Error opening file "
		          << filePath.string()
				  << ": Creating a new file with default content..."
				  << std::endl;

		Logger::log("Error opening file "
					+ filePath.string()
					+ ": Creating a new file with default content..."
					+ "\n");

		std::ofstream fileStream(filePath);
		if (fileStream.is_open())
		{
			fileStream << R"CSV(
# ============================================================================
# HPR Categories Configuration File
#
# HOW TO ADD NEW APPS:
# 1. The format is exactly: raw name,CATEGORY (raw name must be fully lowercase
#    and must be contained in whatever you see in the ui)
# 2. HPR uses "substring matching"! If you add `code,Visual Studio`, it will
#    automatically catch `code`, `vscode`, `code.exe`, `code-oss`, etc.
# 3. Do NOT put spaces around the comma unless you specifically want them!
# 4. Lines starting with # (like this one) or blank lines are safely ignored.
# VALID CATEGORIES:
# WORK
# SOCIAL
# DISTRACTION
# BROWSER
# SYSTEM
# UNKNOWN
# ============================================================================

chrome,BROWSER
google,BROWSER
chromium,BROWSER
msedge,BROWSER
edge,BROWSER
firefox,BROWSER
brave,BROWSER
zen,BROWSER

code,WORK
vscodium,WORK
devenv,WORK
obsidian,WORK
postman,WORK
notion,WORK
notepad,WORK
antigravity,WORK
slack,WORK
teams,WORK
zoom,WORK
texteditor,WORK
text-editor,WORK
terminal,WORK
cmd,WORK
powershell,WORK
pwsh,WORK
ptyxis,WORK
konsole,WORK
tilix,WORK
alacritty,WORK
kitty,WORK
hyper,WORK
terminator,WORK
guake,WORK
tilda,WORK
wezterm,WORK
clion,WORK
dataspell,WORK
webstorm,WORK
phpstorm,WORK
pycharm,WORK

explorer,SYSTEM
dolphin,SYSTEM
nautilus,SYSTEM
thunar,SYSTEM
nemo,SYSTEM
btop,SYSTEM
taskmgr,SYSTEM
systemmonitor,SYSTEM

obs,SOCIAL
spotify,SOCIAL
discord,SOCIAL

steam,DISTRACTION
vlc,DISTRACTION
mpv,DISTRACTION
)CSV";
			fileStream.close();
			std::cerr << "Categories file not found. Created default categories.csv at " << filePath.string() << "\n";
			Logger::log("Categories file not found. Created default categories.csv at " + filePath.string());
		}
		else
		{
			std::cerr << "Error: Could not create default Categories file at " << filePath.string() << "\n";
			Logger::log("Error: Could not create default Categories file at " + filePath.string());
			return;
		}
	}
}

std::vector<int> PatternAnalyzer::calculateProductivityScore(bool todayOnly, bool forceRecalculation)
{
    /* States:
        737:   Not Computed, 
        !737: Cached
    */

    // I know its inefficient and shit
    // but i will refactor it later: 04:42 PM, 26 August 2026 ~Plexescor
    if (productivityFunctionCalls > 8)
    {
        productivityFunctionCalls = 0;
        productivityScore = 737;
    }

    if (productivityScore != 737 && !forceRecalculation)
    {
        productivityFunctionCalls++;
        return productivityScoreCache;
    }

    if (multiDayData_.empty())
    {
        productivityScore = 0;
        productivityScoreCache = {};
        return productivityScoreCache;
    }

    uint64_t minSessionMs = static_cast<uint64_t>(
        AppState::configManager.getConfig("pattern-focus-min-session-ms", 60000));
    double focusCap    = AppState::configManager.getConfig("pattern-focus-cap-mins", 90.0);
    double switchFloor = AppState::configManager.getConfig("pattern-switch-rate-floor", 40.0);

    size_t startIdx = 0;
    size_t endIdx   = todayOnly ? 1 : multiDayData_.size();

    std::vector<int> scores;

    for (size_t i = startIdx; i < endIdx; ++i)
    {
        const auto &day = multiDayData_[i];

        uint64_t totalTime = 0;
        uint64_t workTime  = 0;
        for (const auto &[app, dur] : day.timePerApp)
        {
            totalTime += dur;
            if (getCategory(app) == AppCategory::WORK)
                workTime += dur;
        }

        if (totalTime == 0) { scores.push_back(0); continue; }

        auto sessions = buildSessions(day.switchHistory, minSessionMs);
        double avgSessionMins = 0.0;
        if (!sessions.empty())
        {
            uint64_t totalDur = 0;
            for (const auto &s : sessions)
                totalDur += s.duration;
            avgSessionMins = static_cast<double>(totalDur) / sessions.size() / 60000.0;
        }

        uint64_t earliest = UINT64_MAX, latest = 0;
        int totalSwitches = 0;
        for (const auto &[apps, vec] : day.switchHistory)
        {
            totalSwitches += static_cast<int>(vec.size());
            for (uint64_t ts : vec)
            {
                if (ts < earliest) earliest = ts;
                if (ts > latest)   latest   = ts;
            }
        }

        double activeHours = (earliest == UINT64_MAX || latest <= earliest)
            ? 0.1
            : std::max((latest - earliest) / 3600000.0, 0.1);

        double workScore   = std::min((static_cast<double>(workTime) / totalTime) / 0.70, 1.0) * 100.0;
        double focusScore  = std::min(avgSessionMins / focusCap, 1.0) * 100.0;
        double switchScore = std::max(0.0, 1.0 - (totalSwitches / activeHours / switchFloor)) * 100.0;

        double raw = (workScore * 0.50) + (focusScore * 0.25) + (switchScore * 0.25);
        scores.push_back(static_cast<int>(std::round(std::min(raw, 100.0))));
    }

    productivityScore      = scores.empty() ? 0 : scores.back();
    productivityScoreCache = scores;
    return productivityScoreCache;
}

void PatternAnalyzer::initialiseCategories()
{
	std::ifstream file(filePath);
	std::string line;

	// One line at a time
	while (std::getline(file, line))
	{
		// if empty or starts with #, continue, write comments with #
		if (line.empty() || line.starts_with("#"))
			continue;

		// find comma pos
		size_t commaPos = line.find(',');

		// If it has a comma then do this
		if (commaPos != std::string::npos)
		{
			categoryData[line.substr(0, commaPos)] = line.substr(commaPos + 1);
		}
	}
}
void PatternAnalyzer::generateInsights()
{
	// Make a copy of appstate's map into this
	{
		std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);

		if (AppState::state.currentView == AppState::CurrentView::LIVE)
		{
			timeLog_PerApp = AppState::state.timeLog_PerApp;
			switchHistory = AppState::state.switchHistory;
		}
		else if (AppState::state.currentView == AppState::CurrentView::HISTORICAL_SINGULAR)
		{
			std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
			timeLog_PerApp = AppState::historicalData_State.timeLog_PerApp;
			switchHistory = AppState::historicalData_State.switchHistory;
		}
		else
		{
			std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
			timeLog_PerApp = AppState::historicalData_Full_State.timeLog_PerApp;
			switchHistory = AppState::historicalData_Full_State.switchHistory;
		}
	}

	//PATTERN 0: Productivty Score of today
	if (!timeLog_PerApp.empty())
	{
		auto scores = calculateProductivityScore(true, false);
		int score = scores.empty() ? 0 : scores[0];

		// auto pick = [](int n) { return static_cast<int>(std::rand() % n); };

		static std::string cachedMsg;
		static std::time_t lastPicked = 0;
		static int lastScore = -1;

		if (std::time(nullptr) - lastPicked >= 30 || score != lastScore)
		{
			auto pick = [](int n) { return static_cast<int>(std::rand() % n); };
			char buf[192];
			if (score == 100)
			{
				switch (pick(6))
				{
				case 0: std::snprintf(buf, sizeof(buf), "Impossible. how. what."); break;
				case 1: std::snprintf(buf, sizeof(buf), "The algorithm says 100. the algorithm has never been wrong. today it might be."); break;
				case 2: std::snprintf(buf, sizeof(buf), "Either you had the most focused day of your life or something is broken"); break;
				case 3: std::snprintf(buf, sizeof(buf), "100/100. go outside. you've earned it."); break;
				case 4: std::snprintf(buf, sizeof(buf), "Maybe something broke in HPR...."); break;
				default: std::snprintf(buf, sizeof(buf), "You achieved perfection. this message will never appear again."); break;
				}
			}
			else if (score >= 90)
			{
				switch (pick(5))
				{
				case 0: std::snprintf(buf, sizeof(buf), "Elite behavior. who are you and what did you do with Plexescor"); break;
				case 1: std::snprintf(buf, sizeof(buf), "This is genuinely rare, don't waste it"); break;
				case 2: std::snprintf(buf, sizeof(buf), "You were locked in like the deadline was yesterday"); break;
				case 3: std::snprintf(buf, sizeof(buf), "Close to perfect. annoyingly close."); break;
				default: std::snprintf(buf, sizeof(buf), "HPR has never been more proud"); break;
				}
			}
			else if (score >= 80)
			{
				switch (pick(5))
				{
				case 0: std::snprintf(buf, sizeof(buf), "Actually impressive, what got into you"); break;
				case 1: std::snprintf(buf, sizeof(buf), "This is top 20%% territory"); break;
				case 2: std::snprintf(buf, sizeof(buf), "You barely touched Discord today didn't you"); break;
				case 3: std::snprintf(buf, sizeof(buf), "The sessions were long, the switches were few"); break;
				default: std::snprintf(buf, sizeof(buf), "Save this screenshot for when you need motivation"); break;
				}
			}
			else if (score >= 70)
			{
				switch (pick(5))
				{
				case 0: std::snprintf(buf, sizeof(buf), "Legitimately good day"); break;
				case 1: std::snprintf(buf, sizeof(buf), "You were in the zone for a real chunk of this"); break;
				case 2: std::snprintf(buf, sizeof(buf), "This is what a productive day looks like"); break;
				case 3: std::snprintf(buf, sizeof(buf), "Focus was strong, distractions were managed"); break;
				default: std::snprintf(buf, sizeof(buf), "HPR approves of this day"); break;
				}
			}
			else if (score >= 60)
			{
				switch (pick(5))
				{
				case 0: std::snprintf(buf, sizeof(buf), "Just above the productive threshold, barely counts"); break;
				case 1: std::snprintf(buf, sizeof(buf), "A genuinely decent day ngl"); break;
				case 2: std::snprintf(buf, sizeof(buf), "You locked in more than you slacked off"); break;
				case 3: std::snprintf(buf, sizeof(buf), "Not a highlight reel day but solid"); break;
				default: std::snprintf(buf, sizeof(buf), "63 is the cutoff and you cleared it. respect."); break;
				}
			}
			else if (score >= 50)
			{
				switch (pick(5))
				{
				case 0: std::snprintf(buf, sizeof(buf), "Dead average. perfectly mediocre."); break;
				case 1: std::snprintf(buf, sizeof(buf), "Half your day was real, other half was a blur"); break;
				case 2: std::snprintf(buf, sizeof(buf), "You did okay. okay is fine. okay is not great."); break;
				case 3: std::snprintf(buf, sizeof(buf), "The balance between work and chaos was 50/50 today"); break;
				default: std::snprintf(buf, sizeof(buf), "Median human behavior. congrats."); break;
				}
			}
			else if (score >= 40)
			{
				switch (pick(5))
				{
				case 0: std::snprintf(buf, sizeof(buf), "Almost at the halfway mark, almost"); break;
				case 1: std::snprintf(buf, sizeof(buf), "You're flirting with productivity but not committing"); break;
				case 2: std::snprintf(buf, sizeof(buf), "Solid attempt, questionable execution"); break;
				case 3: std::snprintf(buf, sizeof(buf), "The focus was there in bursts"); break;
				default: std::snprintf(buf, sizeof(buf), "Not bad, not good, aggressively mid"); break;
				}
			}
			else if (score >= 30)
			{
				switch (pick(5))
				{
				case 0: std::snprintf(buf, sizeof(buf), "Below average but not embarrassing"); break;
				case 1: std::snprintf(buf, sizeof(buf), "You had a real session in there somewhere"); break;
				case 2: std::snprintf(buf, sizeof(buf), "More than yesterday probably. maybe."); break;
				case 3: std::snprintf(buf, sizeof(buf), "The grind is present but very inconsistent"); break;
				default: std::snprintf(buf, sizeof(buf), "One good hour hiding in a bad day"); break;
				}
			}
			else if (score >= 20)
			{
				switch (pick(5))
				{
				case 0: std::snprintf(buf, sizeof(buf), "You did some things. a few things."); break;
				case 1: std::snprintf(buf, sizeof(buf), "Getting there. very slowly. but getting there"); break;
				case 2: std::snprintf(buf, sizeof(buf), "Yea bro you need to work hard this isn't getting you anywhere"); break;
				case 3: std::snprintf(buf, sizeof(buf), "You had moments today. brief ones."); break;
				default: std::snprintf(buf, sizeof(buf), "A quarter productive at best"); break;
				}
			}
			else if (score >= 10)
			{
				switch (pick(5))
				{
				case 0: std::snprintf(buf, sizeof(buf), "Rough day or just warming up"); break;
				case 1: std::snprintf(buf, sizeof(buf), "You opened the right apps at least"); break;
				case 2: std::snprintf(buf, sizeof(buf), "Something happened today. not much, but something"); break;
				case 3: std::snprintf(buf, sizeof(buf), "The intention was there, execution not so much"); break;
				default: std::snprintf(buf, sizeof(buf), "Low score but respect for even tracking this"); break;
				}
			}
			else if (score >= 5)
			{
				switch (pick(5))
				{
				case 0: std::snprintf(buf, sizeof(buf), "You showed up. barely, but you showed up"); break;
				case 1: std::snprintf(buf, sizeof(buf), "The vibes were there, the work wasn't"); break;
				case 2: std::snprintf(buf, sizeof(buf), "Technically not zero. that's all we can say"); break;
				case 3: std::snprintf(buf, sizeof(buf), "Your chair got more use than your brain today"); break;
				default: std::snprintf(buf, sizeof(buf), "A heroic effort to do almost nothing"); break;
				}
			}
			else
			{
				switch (pick(5))
				{
				case 0: std::snprintf(buf, sizeof(buf), "Bro did you even turn the monitor on"); break;
				case 1: std::snprintf(buf, sizeof(buf), "This is basically a screen saver score"); break;
				case 2: std::snprintf(buf, sizeof(buf), "Your laptop worked harder than you today"); break;
				case 3: std::snprintf(buf, sizeof(buf), "Are you sure HPR is tracking the right person"); break;
				default: std::snprintf(buf, sizeof(buf), "0 is right there and you still missed it"); break;
				}
			}
			cachedMsg  = buf;
			lastPicked = std::time(nullptr);
			lastScore  = score;
		}

		productivityScore_O = std::to_string(score) + " — " + cachedMsg;
	}

	// PATTERN 1: MOST USED APP
	if (!timeLog_PerApp.empty())
	{
		auto maxIt = std::max_element(timeLog_PerApp.begin(), timeLog_PerApp.end(),
									  [](const auto &a, const auto &b) { return a.second < b.second; });
		std::string name = AppState::aliasManager.getAlias(maxIt->first);
		std::string time = formatTime_HHMMSS(maxIt->second);

		mostUsed_O = name + " — " + time; // i intentionally used —, no ai
	}

	// PATTERN 2: Total tracked time
	if (!timeLog_PerApp.empty())
	{
		uint64_t totalTrackedTime = 0;
		for (const auto &[raw, duration] : timeLog_PerApp)
		{
			totalTrackedTime += duration;
		}

		std::string time = formatTime_HHMMSS(totalTrackedTime);
		totalTrackedTime_O = time;
	}

	// PATTERN 3: Total app switches
	if (!switchHistory.empty())
	{
		size_t total = 0;
		for (const auto &[key, vec] : switchHistory)
		{
			total += vec.size();
		}
		switchCount_O = std::to_string(total);
	}

	// PATTERN 4: Most Switched-Away From App
	if (!switchHistory.empty())
	{
		std::map<std::string, size_t> switchCounts;
		const std::string selfApp = "HPR";
		for (const auto &[apps, vec] : switchHistory)
		{
			const std::string &fromApp = apps.first;
			if (fromApp == selfApp)
				continue;
			switchCounts[fromApp] += vec.size();
		}
		auto maxIt = std::max_element(switchCounts.begin(), switchCounts.end(),
									  [](const auto &a, const auto &b) { return a.second < b.second; });

		std::string app = AppState::aliasManager.getAlias(maxIt->first);
		size_t count = maxIt->second;

		mostSwitchedFrom_O = app + " — " + std::to_string(count) + " switches";
	}

	// PATTERN 5: Most Switched-To From App
	if (!switchHistory.empty())
	{
		std::map<std::string, size_t> switchCounts;
		const std::string selfApp = "HPR";

		for (const auto &[apps, vec] : switchHistory)
		{
			const std::string &toApp = apps.second;
			if (toApp == selfApp)
				continue; // skip switches back to HPR
			switchCounts[toApp] += vec.size();
		}

		auto maxIt = std::max_element(switchCounts.begin(), switchCounts.end(),
									  [](const auto &a, const auto &b) { return a.second < b.second; });

		std::string app = AppState::aliasManager.getAlias(maxIt->first);
		size_t count = maxIt->second;

		mostSwitchedTo_O = app + " — " + std::to_string(count) + " switches";
	}

	// Pattern 6: Longest Focus Session (Robust Chronological Matching)
	if (!switchHistory.empty())
	{
		struct Event
		{
			uint64_t ts;
			std::string app;
			bool isArrival;
		};
		std::vector<Event> events;

		// Use multiple possible names for the HPR window to be safe
		auto isSelf = [](const std::string &name) { return name == "HPR" || name == "Unknown" || name.empty(); };

		for (const auto &[apps, vec] : switchHistory)
		{
			for (uint64_t ts : vec)
			{
				if (!isSelf(apps.first))
					events.push_back({ts, apps.first, false});
				if (!isSelf(apps.second))
					events.push_back({ts, apps.second, true});
			}
		}

		// Sort all switch events by time
		std::sort(events.begin(), events.end(),
				  [](const Event &a, const Event &b)
				  {
					  if (a.ts != b.ts)
						  return a.ts < b.ts;
					  return a.isArrival < b.isArrival; // Process departures before arrivals
														// if same ms
				  });

		uint64_t bestDuration = 0;
		std::string bestApp;
		std::map<std::string, uint64_t> activeSessions;

		for (const auto &e : events)
		{
			if (e.isArrival)
			{
				if (activeSessions.count(e.app))
				{
					activeSessions.erase(e.app); // Discard old arrival that had no departure
				}
				activeSessions[e.app] = e.ts;
			}
			else
			{
				if (activeSessions.count(e.app))
				{
					uint64_t duration = (e.ts >= activeSessions[e.app]) ? (e.ts - activeSessions[e.app]) : 0;

					if (duration > 1000 && duration < (8ULL * 60 * 60 * 1000))
					{
						if (duration > bestDuration)
						{
							bestDuration = duration;
							bestApp = e.app;
						}
					}
					activeSessions.erase(e.app);
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

	// Pattern 7: Peak Productive Hour
	if (!switchHistory.empty())
	{
		std::vector<uint64_t> timestamps;

		// flatten all timestamps into one vector
		for (const auto &[key, vec] : switchHistory)
		{
			timestamps.insert(timestamps.end(), vec.begin(), vec.end());
		}

		std::sort(timestamps.begin(), timestamps.end());

		// window must be between 60 and 90 mins c epochs are milliseconds
		const uint64_t minWindow_ms = 60ULL * 60 * 1000;
		const uint64_t maxWindow_ms = 90ULL * 60 * 1000;

		uint64_t bestStart = 0;
		uint64_t bestEnd = 0;

		size_t left = 0;
		size_t bestCount = SIZE_MAX;
		bool foundValid = false;

		// sliding window , fewer switches in window = more focused
		for (size_t right = 0; right < timestamps.size(); ++right)
		{
			// shrink window from left if it exceeds 90 mins
			while (timestamps[right] - timestamps[left] > maxWindow_ms)
			{
				++left;
			}

			uint64_t windowSpan = timestamps[right] - timestamps[left];

			// only consider windows that are at least 60 mins wide
			if (windowSpan < minWindow_ms)
				continue;

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

AppCategory PatternAnalyzer::getCategory(const std::string appName)
{
	std::string lower = appName;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

	std::string categoryString = "";
	try
	{
		categoryString = categoryData.at(lower);
	}
	catch(const std::out_of_range &e) {/* Not even worth*/ }

	if (categoryString.contains("WORK")) return AppCategory::WORK;
	else if (categoryString.contains("SOCIAL")) return AppCategory::SOCIAL;
	else if (categoryString.contains("DISTRACTION")) return AppCategory::DISTRACTION;
	else if (categoryString.contains("BROWSER")) return AppCategory::BROWSER;
	else if (categoryString.contains("SYSTEM")) return AppCategory::SYSTEM;
	else return AppCategory::UNKNOWN;
}


// Returns a stable-seeded random index 0..n-1 for variant picking.
// Re-seeded each call so successive insights in the same run can differ.
static int pick(int n) { return static_cast<int>(std::rand() % n); }

// Returns the hour-of-day (0-23, local time) for a ms-since-epoch timestamp.
int localHour(uint64_t ms)
{
	time_t t = static_cast<time_t>(ms / 1000);
	std::tm lt = safe_localtime(t);
	return lt.tm_hour;
}

// Returns the day-of-week (0=Sunday … 6=Saturday) for a ms-since-epoch
// timestamp.
int localDow(uint64_t ms)
{
	time_t t = static_cast<time_t>(ms / 1000);
	std::tm lt = safe_localtime(t);
	return lt.tm_wday;
}

static const char *dowName(int dow)
{
	static const char *names[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
	return (dow >= 0 && dow <= 6) ? names[dow] : "Unknown";
}



void PatternAnalyzer::setMultiDayData(std::vector<DayData> data) { multiDayData_ = std::move(data); }

/* AIM
	"You escape to Discord from VSCode ~4 times a day"
	"You almost never return to VSCode after opening YouTube"
	"Your average focus session is 23 minutes before switching"
	"Your most distracted day is usually Tuesday"
	"You've had 5 productive days this week"
	"Your screen time is 30% higher than your weekly average today"
	"After lunch you almost always lose focus for ~40 minutes"
	"73% of your deep work happens before 1PM"
	"Weekends you barely use VSCode"
*/

void PatternAnalyzer::generateAdvancedInsights()
{
	if (multiDayData_.empty())
	{
		escapePattern_O = "Not enough data";
		returnRate_O = "Not enough data";
		avgFocusSession_O = "Not enough data";
		mostDistractedDay_O = "Not enough data";
		productiveDaysThisWeek_O = "Not enough data";
		screenTimeVsAverage_O = "Not enough data";
		focusDipHour_O = "Not enough data";
		deepWorkRelativeNoon_O = "Not enough data";
		weekendVsWeekday_O = "Not enough data";
		return;
	}

	// Config-driven tunables
	std::string cfgWorkApp = AppState::configManager.getConfig<std::string>("pattern-work-app", "");
	std::string cfgBrowserApp = AppState::configManager.getConfig<std::string>("pattern-browser-app", "");
	int switchThreshold = AppState::configManager.getConfig("pattern-productive-switch-threshold", 10);
	uint64_t minSessionMs =
		static_cast<uint64_t>(AppState::configManager.getConfig("pattern-focus-min-session-ms", 60000));

	// Auto-detect dominant work & browser apps across all days if not in config
	std::string workApp, browserApp;

	{
		std::map<std::string, uint64_t> totalTime;
		for (const auto &day : multiDayData_)
			for (const auto &[app, dur] : day.timePerApp)
				totalTime[app] += dur;

		if (!cfgWorkApp.empty())
		{
			workApp = cfgWorkApp;
		}
		else
		{
			// Highest time app that we categorise as WORK
			uint64_t best = 0;
			for (const auto &[app, dur] : totalTime)
			{
				if (getCategory(app) == AppCategory::WORK && dur > best)
				{
					best = dur;
					workApp = app;
				}
			}
			if (workApp.empty())
			{
				// Fallback: just the top app overall
				auto it = std::max_element(totalTime.begin(), totalTime.end(),
										   [](const auto &a, const auto &b) { return a.second < b.second; });
				if (it != totalTime.end())
					workApp = it->first;
			}
		}

		if (!cfgBrowserApp.empty())
		{
			browserApp = cfgBrowserApp;
		}
		else
		{
			uint64_t best = 0;
			for (const auto &[app, dur] : totalTime)
			{
				if (getCategory(app) == AppCategory::BROWSER && dur > best)
				{
					best = dur;
					browserApp = app;
				}
			}
		}
	}

	std::string workAlias = workApp.empty() ? "your work app" : AppState::aliasManager.getAlias(workApp);
	std::string browserAlias = browserApp.empty() ? "your browser" : AppState::aliasManager.getAlias(browserApp);

	// Escape pattern: avg WORK→BROWSER switches per day
	{
		double totalEscapes = 0.0;
		int daysWithWork = 0;

		for (const auto &day : multiDayData_)
		{
			int dayEscapes = 0;
			for (const auto &[apps, vec] : day.switchHistory)
			{
				bool fromWork = (!workApp.empty() && apps.first.find(workApp) != std::string::npos) ||
								getCategory(apps.first) == AppCategory::WORK;
				bool toBrowser = (!browserApp.empty() && apps.second.find(browserApp) != std::string::npos) ||
								 getCategory(apps.second) == AppCategory::BROWSER;
				if (fromWork && toBrowser)
					dayEscapes += static_cast<int>(vec.size());
			}
			if (dayEscapes > 0)
			{
				totalEscapes += dayEscapes;
				++daysWithWork;
			}
		}

		if (daysWithWork == 0)
		{
			escapePattern_O = "No escape switches detected yet";
		}
		else
		{
			double avg = totalEscapes / daysWithWork;
			char buf[160];
			std::srand(static_cast<unsigned>(std::time(nullptr)) ^ 0x1A);
			switch (pick(5))
			{
			case 0:
				std::snprintf(buf, sizeof(buf), "You escape to %s from %s ~%.0f times a day", browserAlias.c_str(),
							  workAlias.c_str(), avg);
				break;
			case 1:
				std::snprintf(buf, sizeof(buf), "%s pulls you away from %s about %.0f times a day",
							  browserAlias.c_str(), workAlias.c_str(), avg);
				break;
			case 2:
				std::snprintf(buf, sizeof(buf), "You ditch %s for %s roughly %.0f times daily", workAlias.c_str(),
							  browserAlias.c_str(), avg);
				break;
			case 3:
				std::snprintf(buf, sizeof(buf), "On average you switch to %s from %s %.0f time%s each day",
							  browserAlias.c_str(), workAlias.c_str(), avg, avg < 1.5 ? "" : "s");
				break;
			default:
				std::snprintf(buf, sizeof(buf), "About %.0f time%s a day %s loses you to %s", avg, avg < 1.5 ? "" : "s",
							  workAlias.c_str(), browserAlias.c_str());
				break;
			}
			escapePattern_O = buf;
		}
	}

	// Return rate: after work→browser, what % go back to work?
	{
		int escapes = 0;
		int returns = 0;

		for (const auto &day : multiDayData_)
		{
			// Build a flat sorted event list: {ts /* this shit lol*/ , from, to}
			struct Sw
			{
				uint64_t ts;
				std::string from;
				std::string to;
			};
			std::vector<Sw> events;
			for (const auto &[apps, vec] : day.switchHistory)
				for (uint64_t ts : vec)
					events.push_back({ts, apps.first, apps.second});
			std::sort(events.begin(), events.end(), [](const Sw &a, const Sw &b) { return a.ts < b.ts; });

			for (size_t i = 0; i < events.size(); ++i)
			{
				bool fromWork = (!workApp.empty() && events[i].from.find(workApp) != std::string::npos) ||
								getCategory(events[i].from) == AppCategory::WORK;
				bool toBrowser = (!browserApp.empty() && events[i].to.find(browserApp) != std::string::npos) ||
								 getCategory(events[i].to) == AppCategory::BROWSER;

				if (fromWork && toBrowser)
				{
					++escapes;
					// Look at the very next switch did it go back to work?
					if (i + 1 < events.size())
					{
						bool nextFromBrowser =
							(!browserApp.empty() && events[i + 1].from.find(browserApp) != std::string::npos) ||
							getCategory(events[i + 1].from) == AppCategory::BROWSER;
						bool nextToWork = (!workApp.empty() && events[i + 1].to.find(workApp) != std::string::npos) ||
										  getCategory(events[i + 1].to) == AppCategory::WORK;
						if (nextFromBrowser && nextToWork)
							++returns;
					}
				}
			}
		}

		if (escapes == 0)
		{
			returnRate_O = "No browser escapes recorded yet";
		}
		else
		{
			int pct = static_cast<int>(100.0 * returns / escapes);
			char buf[192];
			std::srand(static_cast<unsigned>(std::time(nullptr)) ^ 0x2B);
			switch (pick(5))
			{
			case 0:
				std::snprintf(buf, sizeof(buf),
							  "After switching to %s, you return to %s only "
							  "%d%% of the time",
							  browserAlias.c_str(), workAlias.c_str(), pct);
				break;
			case 1:
				std::snprintf(buf, sizeof(buf), "Only %d%% of the time does %s bring you back to %s", pct,
							  browserAlias.c_str(), workAlias.c_str());
				break;
			case 2:
				std::snprintf(buf, sizeof(buf),
							  "You find your way back to %s just %d%% of the time "
							  "after opening %s",
							  workAlias.c_str(), pct, browserAlias.c_str());
				break;
			case 3:
				std::snprintf(buf, sizeof(buf), "%d%% of your %s breaks end with you returning to %s", pct,
							  browserAlias.c_str(), workAlias.c_str());
				break;
			default:
				std::snprintf(buf, sizeof(buf),
							  "Once you open %s, there's only a %d%% chance "
							  "you go back to %s",
							  browserAlias.c_str(), pct, workAlias.c_str());
				break;
			}
			returnRate_O = buf;
		}
	}

	// Average focus session length across all days
	{
		uint64_t totalDur = 0;
		int count = 0;

		for (const auto &day : multiDayData_)
		{
			auto sessions = buildSessions(day.switchHistory, minSessionMs);
			for (const auto &s : sessions)
			{
				totalDur += s.duration;
				++count;
			}
		}

		if (count == 0)
		{
			avgFocusSession_O = "Not enough focus sessions yet";
		}
		else
		{
			int avgMin = static_cast<int>((totalDur / count) / 60000);
			const char *pl = avgMin == 1 ? "" : "s";
			char buf[160];
			std::srand(static_cast<unsigned>(std::time(nullptr)) ^ 0x3C);
			switch (pick(5))
			{
			case 0:
				std::snprintf(buf, sizeof(buf),
							  "Your average focus session is %d minute%s "
							  "before you switch",
							  avgMin, pl);
				break;
			case 1:
				std::snprintf(buf, sizeof(buf),
							  "You stay focused for about %d minute%s on average "
							  "before switching away",
							  avgMin, pl);
				break;
			case 2:
				std::snprintf(buf, sizeof(buf),
							  "On average you hold focus for %d minute%s before "
							  "jumping to something else",
							  avgMin, pl);
				break;
			case 3:
				std::snprintf(buf, sizeof(buf),
							  "Your typical uninterrupted stretch lasts around "
							  "%d minute%s",
							  avgMin, pl);
				break;
			default:
				std::snprintf(buf, sizeof(buf),
							  "It takes about %d minute%s before you switch apps on an "
							  "average session",
							  avgMin, pl);
				break;
			}
			avgFocusSession_O = buf;
		}
	}

	// Most distracted day of week (highest avg switches)
	{
		// Map DOW → list of per-day switch totals
		std::map<int, std::vector<int>> switchesByDow;

		for (const auto &day : multiDayData_)
		{
			// Parse date DDMMYY → get a representative timestamp for DOW
			// Use the first switch timestamp if available, else skip
			uint64_t repTs = 0;
			for (const auto &[apps, vec] : day.switchHistory)
			{
				if (!vec.empty())
				{
					repTs = vec.front();
					break;
				}
			}
			if (repTs == 0)
				continue;

			int dow = localDow(repTs);
			int total = 0;
			for (const auto &[apps, vec] : day.switchHistory)
				total += static_cast<int>(vec.size());

			switchesByDow[dow].push_back(total);
		}

		if (switchesByDow.empty())
		{
			mostDistractedDay_O = "Not enough data across days";
		}
		else
		{
			int worstDow = 0;
			double worstAvg = -1.0;
			for (const auto &[dow, counts] : switchesByDow)
			{
				double avg = 0;
				for (int c : counts)
					avg += c;
				avg /= counts.size();
				if (avg > worstAvg)
				{
					worstAvg = avg;
					worstDow = dow;
				}
			}
			char buf[192];
			const char *dn = dowName(worstDow);
			std::srand(static_cast<unsigned>(std::time(nullptr)) ^ 0x4D);
			switch (pick(5))
			{
			case 0:
				std::snprintf(buf, sizeof(buf),
							  "%s is your most distracted day — you switch apps ~%.0f "
							  "times on average",
							  dn, worstAvg);
				break;
			case 1:
				std::snprintf(buf, sizeof(buf),
							  "You're at your most scattered on %ss — averaging ~%.0f "
							  "app switches",
							  dn, worstAvg);
				break;
			case 2:
				std::snprintf(buf, sizeof(buf),
							  "%s hits different — you switch apps about %.0f times "
							  "that day on average",
							  dn, worstAvg);
				break;
			case 3:
				std::snprintf(buf, sizeof(buf),
							  "Your focus takes the biggest hit on %ss — ~%.0f "
							  "switches on average",
							  dn, worstAvg);
				break;
			default:
				std::snprintf(buf, sizeof(buf),
							  "%.0f app switches on average — %s is your most "
							  "chaotic day",
							  worstAvg, dn);
				break;
			}
			mostDistractedDay_O = buf;
		}
	}

	// Number of productive days (low switch rate)
	{
		int productivityThreshold = AppState::configManager.getConfig<int>("productivity-score-threshold", 63);

		int productive = 0;
		std::vector<int> scores = calculateProductivityScore(false, false);

		for (auto const xxx : scores)
		{
			if (xxx > productivityThreshold)
			{
				productive++;
			}
		}

		const char *pl2 = productive == 1 ? "" : "s";
		char buf[128];
		std::srand(static_cast<unsigned>(std::time(nullptr)) ^ 0x5E);
		switch (pick(5))
		{
		case 0:
			std::snprintf(buf, sizeof(buf), "You had %d productive day%s this week", productive, pl2);
			break;
		case 1:
			std::snprintf(buf, sizeof(buf), "%d out of %d day%s this week counted as productive", productive,
						  static_cast<int>(multiDayData_.size()), multiDayData_.size() == 1 ? "" : "s");
			break;
		case 2:
			std::snprintf(buf, sizeof(buf), "This week you stayed on track %d day%s", productive, pl2);
			break;
		case 3:
			std::snprintf(buf, sizeof(buf), "%d focused day%s logged this week", productive, pl2);
			break;
		default:
			std::snprintf(buf, sizeof(buf), "You locked in %d day%s this week", productive, pl2);
			break;
		}
		productiveDaysThisWeek_O = buf;
	}

	// Today's screen time vs. N-day average
	{
		if (multiDayData_.empty())
		{
			screenTimeVsAverage_O = "Not enough data";
		}
		else
		{
			// Index 0 = today (most recent day loaded)
			uint64_t todayTotal = 0;
			for (const auto &[app, dur] : multiDayData_[0].timePerApp)
				todayTotal += dur;

			if (multiDayData_.size() < 2)
			{
				screenTimeVsAverage_O = "Need more days to compare";
			}
			else
			{
				uint64_t otherTotal = 0;
				int otherDays = 0;
				for (size_t i = 1; i < multiDayData_.size(); ++i)
				{
					uint64_t daySum = 0;
					for (const auto &[app, dur] : multiDayData_[i].timePerApp)
						daySum += dur;
					if (daySum > 0)
					{
						otherTotal += daySum;
						++otherDays;
					}
				}

				if (otherDays == 0 || otherTotal == 0)
				{
					screenTimeVsAverage_O = "Not enough historical data";
				}
				else
				{
					double avgOther = static_cast<double>(otherTotal) / otherDays;
					int pct =
						static_cast<int>(std::round((static_cast<double>(todayTotal) - avgOther) / avgOther * 100.0));
					char buf[192];
					std::srand(static_cast<unsigned>(std::time(nullptr)) ^ 0x6F);
					int absPct = pct >= 0 ? pct : -pct;
					if (pct >= 0)
					{
						switch (pick(5))
						{
						case 0:
							std::snprintf(buf, sizeof(buf),
										  "Today your screen time is %d%% "
										  "higher than your "
										  "weekly average",
										  absPct);
							break;
						case 1:
							std::snprintf(buf, sizeof(buf),
										  "You're %d%% more active on screen "
										  "today than usual",
										  absPct);
							break;
						case 2:
							std::snprintf(buf, sizeof(buf),
										  "Today is running %d%% heavier than "
										  "your typical day",
										  absPct);
							break;
						case 3:
							std::snprintf(buf, sizeof(buf),
										  "Screen time today is up %d%% from "
										  "your weekly average",
										  absPct);
							break;
						default:
							std::snprintf(buf, sizeof(buf),
										  "You've spent %d%% more time on "
										  "screen today than average",
										  absPct);
							break;
						}
					}
					else
					{
						switch (pick(5))
						{
						case 0:
							std::snprintf(buf, sizeof(buf),
										  "Today your screen time is %d%% "
										  "lower than your "
										  "weekly average",
										  absPct);
							break;
						case 1:
							std::snprintf(buf, sizeof(buf),
										  "You're %d%% lighter on screen today "
										  "than usual",
										  absPct);
							break;
						case 2:
							std::snprintf(buf, sizeof(buf), "Today is running %d%% below your typical day", absPct);
							break;
						case 3:
							std::snprintf(buf, sizeof(buf),
										  "Screen time today is down %d%% from "
										  "your weekly average",
										  absPct);
							break;
						default:
							std::snprintf(buf, sizeof(buf),
										  "You've spent %d%% less time on "
										  "screen today than average",
										  absPct);
							break;
						}
					}
					screenTimeVsAverage_O = buf;
				}
			}
		}
	}

	// Focus dip hour: hour most consistently spiked in switches
	{
		// For each day build hourly switch counts, then find the hour with
		// the highest average switch count across days.
		std::map<int, std::vector<int>> switchesPerHour; // hour → per-day counts

		for (const auto &day : multiDayData_)
		{
			std::map<int, int> hourCount;
			for (const auto &[apps, vec] : day.switchHistory)
				for (uint64_t ts : vec)
					hourCount[localHour(ts)]++;

			for (const auto &[hr, cnt] : hourCount)
				switchesPerHour[hr].push_back(cnt);
		}

		if (switchesPerHour.empty())
		{
			focusDipHour_O = "Not enough data";
		}
		else
		{
			int bestHour = 0;
			double bestAvg = -1.0;
			for (const auto &[hr, counts] : switchesPerHour)
			{
				double avg = 0;
				for (int c : counts)
					avg += c;
				avg /= counts.size();
				if (avg > bestAvg)
				{
					bestAvg = avg;
					bestHour = hr;
				}
			}

			int endHour = (bestHour + 1) % 24;
			auto fmt12 = [](int h) -> std::string
			{
				const char *ampm = h < 12 ? "AM" : "PM";
				int h12 = h % 12;
				if (h12 == 0)
					h12 = 12;
				char tmp[16];
				std::snprintf(tmp, sizeof(tmp), "%d%s", h12, ampm);
				return tmp;
			};
			char buf[192];
			std::string h1 = fmt12(bestHour);
			std::string h2 = fmt12(endHour);
			std::srand(static_cast<unsigned>(std::time(nullptr)) ^ 0x7A);
			switch (pick(5))
			{
			case 0:
				std::snprintf(buf, sizeof(buf), "You lose focus almost every day between %s and %s", h1.c_str(),
							  h2.c_str());
				break;
			case 1:
				std::snprintf(buf, sizeof(buf), "The %s-%s window is where your focus tends to collapse", h1.c_str(),
							  h2.c_str());
				break;
			case 2:
				std::snprintf(buf, sizeof(buf),
							  "Between %s and %s you're consistently at your "
							  "most distracted",
							  h1.c_str(), h2.c_str());
				break;
			case 3:
				std::snprintf(buf, sizeof(buf), "Most days around %s your concentration starts to slip", h1.c_str());
				break;
			default:
				std::snprintf(buf, sizeof(buf), "%s to %s is your daily distraction window", h1.c_str(), h2.c_str());
				break;
			}
			focusDipHour_O = buf;
		}
	}

	// Deep work before noon: % of days where longest session
	//             starts before 12:00 local time
	{
		std::string finalData;
		int daysChecked = 0;
		int daysBeforeNoon = 0;
		int daysAfterNoon = 0;

		for (const auto &day : multiDayData_)
		{
			auto sessions = buildSessions(day.switchHistory, minSessionMs);
			if (sessions.empty())
				continue;

			// Find the longest session
			auto best = std::max_element(sessions.begin(), sessions.end(),
										 [](const Session &a, const Session &b) { return a.duration < b.duration; });

			++daysChecked;
			if (localHour(best->startTs) < 12)
				++daysBeforeNoon;
			if (localHour(best->startTs) > 12)
				++daysAfterNoon;
		}

		if (daysChecked == 0)
		{
			deepWorkRelativeNoon_O = "Not enough session data";
		}
		else
		{
			char buf[160];
			std::srand(static_cast<unsigned>(std::time(nullptr)) ^ 0x8B);
			bool morningDominant = daysBeforeNoon >= daysAfterNoon;
			int pct = morningDominant
				? static_cast<int>(100.0 * daysBeforeNoon / daysChecked)
				: static_cast<int>(100.0 * daysAfterNoon / daysChecked);
			//Wether pick Before or After noon
			switch (pick(2))
			{
				
				//Before noon
				case 0:
					pct = static_cast<int>(100.0 * daysBeforeNoon / daysChecked);
					switch (pick(5))
					{
					case 0:
						std::snprintf(buf, sizeof(buf), "%d%% of your longest focus sessions happen before noon", pct);
						break;
					case 1:
						std::snprintf(buf, sizeof(buf), "Before noon is where %d%% of your best deep work lives", pct);
						break;
					case 2:
						std::snprintf(buf, sizeof(buf),
									"Your peak sessions start before 12PM about %d%% "
									"of the time",
									pct);
						break;
					case 3:
						std::snprintf(buf, sizeof(buf),
									"%d%% of the time your longest streak kicks off "
									"in the morning",
									pct);
						break;
					default:
						std::snprintf(buf, sizeof(buf), "You do your deepest work before noon %d%% of days", pct);
						break;
					}

				//After Noon
				default:
					pct = static_cast<int>(100.0 * daysAfterNoon / daysChecked);
					switch (pick(5))
					{
					case 0:
						std::snprintf(buf, sizeof(buf), "%d%% of your longest focus sessions happen after noon", pct);
						break;
					case 1:
						std::snprintf(buf, sizeof(buf), "After noon is where %d%% of your best deep work lives", pct);
						break;
					case 2:
						std::snprintf(buf, sizeof(buf),
									"Your peak sessions start After 12PM about %d%% "
									"of the time",
									pct);
						break;
					case 3:
						std::snprintf(buf, sizeof(buf),
									"%d%% of the time your longest streak kicks off "
									"in the evening",
									pct);
						break;
					default:
						std::snprintf(buf, sizeof(buf), "You do your deepest work after noon %d%% of days", pct);
						break;
					}	
			}
			
			deepWorkRelativeNoon_O = buf;
		}
	}

	// Weekend vs. weekday usage of primary work app
	{
		uint64_t weekdayTotal = 0, weekendTotal = 0;
		int weekdayDays = 0, weekendDays = 0;

		for (const auto &day : multiDayData_)
		{
			// Get a representative timestamp from any switch event
			uint64_t repTs = 0;
			for (const auto &[apps, vec] : day.switchHistory)
			{
				if (!vec.empty())
				{
					repTs = vec.front();
					break;
				}
			}
			if (repTs == 0)
				continue;

			int dow = localDow(repTs);
			bool isWeekend = (dow == 0 || dow == 6);

			uint64_t appTime = 0;
			if (!workApp.empty())
			{
				auto it = day.timePerApp.find(workApp);
				if (it != day.timePerApp.end())
					appTime = it->second;
			}
			else
			{
				// fallback: total screen time
				for (const auto &[app, dur] : day.timePerApp)
					appTime += dur;
			}

			if (isWeekend)
			{
				weekendTotal += appTime;
				++weekendDays;
			}
			else
			{
				weekdayTotal += appTime;
				++weekdayDays;
			}
		}

		if (weekdayDays == 0 || weekdayTotal == 0)
		{
			weekendVsWeekday_O = "Not enough weekday data yet";
		}
		else
		{
			double avgWeekday = static_cast<double>(weekdayTotal) / weekdayDays;
			double avgWeekend = (weekendDays > 0) ? static_cast<double>(weekendTotal) / weekendDays : 0.0;

			int pct = static_cast<int>(std::round(std::abs(avgWeekday - avgWeekend) / avgWeekday * 100.0));

			char buf[192];
			std::srand(static_cast<unsigned>(std::time(nullptr)) ^ 0x9C);
			if (avgWeekend < avgWeekday)
			{
				switch (pick(5))
				{
				case 0:
					std::snprintf(buf, sizeof(buf),
								  "On weekends you spend %d%% less time on %s "
								  "than weekdays",
								  pct, workAlias.c_str());
					break;
				case 1:
					std::snprintf(buf, sizeof(buf), "Your %s usage drops %d%% on weekends", workAlias.c_str(), pct);
					break;
				case 2:
					std::snprintf(buf, sizeof(buf), "Weekends mean %d%% less %s for you", pct, workAlias.c_str());
					break;
				case 3:
					std::snprintf(buf, sizeof(buf), "%s sees %d%% less of you on weekends than weekdays",
								  workAlias.c_str(), pct);
					break;
				default:
					std::snprintf(buf, sizeof(buf), "You're %d%% less likely to use %s over the weekend", pct,
								  workAlias.c_str());
					break;
				}
			}
			else
			{
				switch (pick(5))
				{
				case 0:
					std::snprintf(buf, sizeof(buf),
								  "On weekends you spend %d%% more time on %s "
								  "than weekdays",
								  pct, workAlias.c_str());
					break;
				case 1:
					std::snprintf(buf, sizeof(buf), "Your %s usage actually goes up %d%% on weekends",
								  workAlias.c_str(), pct);
					break;
				case 2:
					std::snprintf(buf, sizeof(buf), "Weekends bring %d%% more %s time for you", pct, workAlias.c_str());
					break;
				case 3:
					std::snprintf(buf, sizeof(buf), "%s sees %d%% more of you on weekends than weekdays",
								  workAlias.c_str(), pct);
					break;
				default:
					std::snprintf(buf, sizeof(buf), "You grind %d%% harder on %s over the weekend", pct,
								  workAlias.c_str());
					break;
				}
			}
			weekendVsWeekday_O = buf;
		}
	}
}
std::string PatternAnalyzer::getProductivityScore() { return productivityScore_O; }
std::string PatternAnalyzer::getMostUsed() { return mostUsed_O; }

std::string PatternAnalyzer::getTotalTrackedTime() { return totalTrackedTime_O; }

std::string PatternAnalyzer::getSwitchCount() { return switchCount_O; }

std::string PatternAnalyzer::getMostSwitchedFrom() { return mostSwitchedFrom_O; }

std::string PatternAnalyzer::getMostSwitchedTo() { return mostSwitchedTo_O; }

std::string PatternAnalyzer::getMostFocusedSession() { return mostFocusedSession_O; }

std::string PatternAnalyzer::getMostProductiveHour() { return mostProductiveHour_O; }

std::string PatternAnalyzer::getEscapePattern() { return escapePattern_O; }
std::string PatternAnalyzer::getReturnRate() { return returnRate_O; }
std::string PatternAnalyzer::getAvgFocusSession() { return avgFocusSession_O; }
std::string PatternAnalyzer::getMostDistractedDay() { return mostDistractedDay_O; }
std::string PatternAnalyzer::getProductiveDaysThisWeek() { return productiveDaysThisWeek_O; }
std::string PatternAnalyzer::getScreenTimeVsAverage() { return screenTimeVsAverage_O; }
std::string PatternAnalyzer::getFocusDipHour() { return focusDipHour_O; }
std::string PatternAnalyzer::getDeepWorkBeforeNoon() { return deepWorkRelativeNoon_O; }
std::string PatternAnalyzer::getWeekendVsWeekday() { return weekendVsWeekday_O; }