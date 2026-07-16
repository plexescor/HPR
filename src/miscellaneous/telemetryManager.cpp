#include "telemetryManager.hpp"
#include "appEvents.hpp"
#include "appState.hpp"
#include "logger.hpp"
#include "netUtilities.hpp"
#include "timeUtils.hpp"

#include <chrono>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <print>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

namespace
{
const std::string FIREBASE_HOST = "humanpatternrecorder-default-rtdb.firebaseio.com";
}

std::string TelemetryManager::generateUUID()
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 15);
	std::uniform_int_distribution<> dis2(8, 11);

	std::stringstream ss;
	ss << std::hex;
	for (int i = 0; i < 8; i++)
		ss << dis(gen);
	ss << "-";
	for (int i = 0; i < 4; i++)
		ss << dis(gen);
	ss << "-4"; // UUID version 4
	for (int i = 0; i < 3; i++)
		ss << dis(gen);
	ss << "-";
	ss << dis2(gen);
	for (int i = 0; i < 3; i++)
		ss << dis(gen);
	ss << "-";
	for (int i = 0; i < 12; i++)
		ss << dis(gen);
	return ss.str();
}

void TelemetryManager::init()
{
	// Run the telemetry check asynchronously in a background thread
	std::thread(
		[]()
		{
			// Wait 5 seconds after startup for database initialization to
			// finish
			std::this_thread::sleep_for(std::chrono::seconds(5));
			checkAndSend();
		})
		.detach();

	// Subscribe to midnight rollover to re-evaluate telemetry when the day
	// transitions
	EventHub::connect(Event::MIDNIGHT_ROLLOVER,
					  [](EventData data)
					  {
						  std::thread(
							  []()
							  {
								  // Wait 5 seconds after rollover for the
								  // database rollover to fully settle
								  std::this_thread::sleep_for(std::chrono::seconds(5));
								  checkAndSend();
							  })
							  .detach();
					  });

	// Privileged aggregation loop — runs immediately on startup, then every 30
	// minutes. Only active when firebase-password is set to a non-default
	// value.
	std::thread(
		[]()
		{
			// Short delay to let app and network settle before first run
			std::this_thread::sleep_for(std::chrono::seconds(8));
			while (true)
			{
				std::string pw = AppState::configManager.getConfig<std::string>("firebase-password", "69");
				if (!pw.empty() && pw != "69")
				{
					privilegedAggregationCycle();
				}
				std::this_thread::sleep_for(std::chrono::minutes(30));
			}
		})
		.detach();
}

void TelemetryManager::checkAndSend()
{
	try
	{
		// Bail out if user hasn't opted in (default is false)
		bool enabled = AppState::configManager.getConfig<bool>("anonymous-telemetry", false);
		if (!enabled)
		{
			// std::println("[Telemetry] Telemetry is disabled. Skipping
			// telemetry check.");
			return;
		}

		// Get or generate UUID
		std::string userId = AppState::configManager.getConfig<std::string>("user-id", "");
		if (userId.empty())
		{
			userId = generateUUID();
			AppState::configManager.setConfig("user-id", userId);
		}

		// 1. Report User Registration (Total User Count)
		// Only send on the very first launch — isFirstLaunch() is true when
		// HPR.lock was created this session (i.e. it didn't exist before this
		// run).
		if (AppState::configManager.isFirstLaunch())
		{
			auto now = std::chrono::system_clock::now();
			uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

			std::string body = "{\"userId\":\"" + userId + "\",\"registeredAt\":" + std::to_string(ts) + "}";
			std::map<std::string, std::string> headers = {{"Content-Type", "application/json"}};

			// PUT with userId as key → same user can never produce duplicate
			// entries
			std::string regPath = "/telemetry/users/" + userId + ".json";
			auto response = NativeNet::httpPut(FIREBASE_HOST, regPath, body, true, headers);
			if (response.second >= 200 && response.second < 300)
			{
				Logger::log("[Telemetry] User registered successfully");
			}
			else
			{
				Logger::log("[Telemetry] User registration failed with code: " + std::to_string(response.second));
			}
		}

		// 2. Report Active Usage (4+ days/week)
		// std::println("[Telemetry] Checking weekly active usage for user: {}",
		// userId);
		auto now = std::chrono::system_clock::now();
		std::time_t nowC = std::chrono::system_clock::to_time_t(now);
		std::tm local_tm = {};
#ifdef _WIN32
		localtime_s(&local_tm, &nowC);
#else
		localtime_r(&nowC, &local_tm);
#endif

		int currentDayOfWeek = local_tm.tm_wday; // 0 = Sunday, 1 = Monday, ..., 6 = Saturday
		int daysSinceMonday = (currentDayOfWeek + 6) % 7;

		// Calculate Monday date string for this week (YYYY-MM-DD)
		std::time_t mondayTime = nowC - daysSinceMonday * 86400;
		std::tm mondayTm = {};
#ifdef _WIN32
		localtime_s(&mondayTm, &mondayTime);
#else
		localtime_r(&mondayTime, &mondayTm);
#endif
		char mondayBuffer[16];
		std::strftime(mondayBuffer, sizeof(mondayBuffer), "%Y-%m-%d", &mondayTm);
		std::string weekId(mondayBuffer);

		// Check active days by verifying the existence of database files
		int activeDaysCount = 0;
		for (int i = 0; i < 7; ++i)
		{
			std::time_t dayTime = nowC + (i - daysSinceMonday) * 86400;
			std::tm dayTm = {};
#ifdef _WIN32
			localtime_s(&dayTm, &dayTime);
#else
			localtime_r(&dayTime, &dayTm);
#endif
			char buf[16];
			std::strftime(buf, sizeof(buf), "%d-%m-%y", &dayTm);
			std::string dateString(buf);

			std::string dbPath;
			std::string MMYY = extractMMYY_from_DDMMYY(dateString);
			if (MMYY.empty())
				continue;

#ifdef _WIN32
			char *appData = std::getenv("APPDATA");
			if (appData)
			{
				dbPath = std::string(appData) + "/HPR/HPR_DB/" + MMYY + "/" + dateString + ".db";
			}
#else
			const char *home = std::getenv("HOME");
			if (home)
			{
				dbPath = std::string(home) + "/.local/share/HPR/HPR_DB/" + MMYY + "/" + dateString + ".db";
			}
#endif

			if (!dbPath.empty() && std::filesystem::exists(dbPath))
			{
				activeDaysCount++;
			}
		}

		// std::println("[Telemetry] User {} has {} active days this week
		// (weekId:
		// {})", userId, activeDaysCount, weekId);
		if (activeDaysCount >= 4)
		{
			std::string lastReportedWeek = AppState::configManager.getConfig<std::string>("last-reported-week", "");
			if (lastReportedWeek != weekId)
			{
				// std::println("[Telemetry] About to send weekly active data
				// for week:
				// {}", weekId);
				std::string body = "{\"userId\":\"" + userId + "\"}";
				std::map<std::string, std::string> headers = {{"Content-Type", "application/json"}};

				// PUT with userId as key under weekId → same user same week =
				// same path = no duplicate
				std::string path = "/telemetry/weekly_active/" + weekId + "/" + userId + ".json";
				// std::println("[Telemetry] Sending PUT to path: {}", path);
				// std::println("[Telemetry] Body: {}", body);
				auto response = NativeNet::httpPut(FIREBASE_HOST, path, body, true, headers);
				if (response.second >= 200 && response.second < 300)
				{
					AppState::configManager.setConfig("last-reported-week", weekId);
					Logger::log("[Telemetry] Weekly active reported "
								"successfully for week: " +
								weekId);
				}
				else
				{
					Logger::log("[Telemetry] Weekly active reporting failed "
								"with code: " +
								std::to_string(response.second));
				}
			}
		}
	}
	catch (const std::exception &e)
	{
		Logger::log("[Telemetry] Error in checkAndSend: " + std::string(e.what()));
	}
	catch (...)
	{
		Logger::log("[Telemetry] Unknown error in checkAndSend");
	}
}

int TelemetryManager::countJsonTopLevelKeys(const std::string &json)
{
	// Counts the number of top-level keys in a flat Firebase JSON object like
	// {"key1":{...},"key2":"val",...}. Uses depth tracking — only counts
	// key-colon patterns at depth 1 (directly inside the outer braces).
	if (json.empty() || json == "null")
		return 0;

	int count = 0;
	int depth = 0;
	bool inString = false;
	bool escaped = false;

	for (size_t i = 0; i < json.size(); ++i)
	{
		char c = json[i];

		if (escaped)
		{
			escaped = false;
			continue;
		}
		if (c == '\\' && inString)
		{
			escaped = true;
			continue;
		}
		if (c == '"')
		{
			inString = !inString;
			continue;
		}
		if (inString)
			continue;

		if (c == '{' || c == '[')
		{
			depth++;
			continue;
		}
		if (c == '}' || c == ']')
		{
			depth--;
			continue;
		}

		// A ':' at depth 1 means we're seeing a top-level key-value separator
		if (c == ':' && depth == 1)
		{
			count++;
		}
	}
	return count;
}

bool TelemetryManager::jsonHasTopLevelKey(const std::string &json, const std::string &key)
{
	// Checks whether a specific key exists at the top level of a Firebase JSON
	// object. Searches for the pattern "key": at depth 1 to avoid false matches
	// inside nested values.
	if (json.empty() || json == "null")
		return false;

	const std::string target = "\"" + key + "\"";
	int depth = 0;
	bool inString = false;
	bool escaped = false;
	size_t i = 0;

	while (i < json.size())
	{
		char c = json[i];

		if (escaped)
		{
			escaped = false;
			++i;
			continue;
		}
		if (c == '\\' && inString)
		{
			escaped = true;
			++i;
			continue;
		}
		if (c == '"')
		{
			inString = !inString;
			if (inString && depth == 1)
			{
				// Check if this quoted string matches the target key
				if (json.compare(i, target.size(), target) == 0)
				{
					// Make sure the next non-space char after closing quote is
					// ':'
					size_t j = i + target.size();
					while (j < json.size() && json[j] == ' ')
						++j;
					if (j < json.size() && json[j] == ':')
						return true;
				}
			}
			++i;
			continue;
		}
		if (inString)
		{
			++i;
			continue;
		}
		if (c == '{' || c == '[')
		{
			depth++;
		}
		else if (c == '}' || c == ']')
		{
			depth--;
		}
		++i;
	}
	return false;
}

int TelemetryManager::parseCountFromSummary(const std::string &json, const std::string &prefix)
{
	if (json.empty() || json == "null")
		return 0;

	// Firebase returns a plain stored string as: "Total Users: 42"
	// Strip the surrounding quotes that the REST API wraps around string
	// values.
	std::string value = json;
	if (!value.empty() && value.front() == '"')
		value = value.substr(1);
	if (!value.empty() && value.back() == '"')
		value.pop_back();

	auto pos = value.find(prefix);
	if (pos == std::string::npos)
		return 0;

	try
	{
		return std::stoi(value.substr(pos + prefix.size()));
	}
	catch (...)
	{
		return 0;
	}
}

std::vector<std::string> TelemetryManager::extractTopLevelKeys(const std::string &json)
{
	// Returns all top-level key names from a flat Firebase JSON object like
	// {"key1":{...},"key2":"val",...}. Uses the same depth-tracking approach as
	// countJsonTopLevelKeys but accumulates key strings instead of counting.
	std::vector<std::string> keys;
	if (json.empty() || json == "null")
		return keys;

	int depth = 0;
	bool inString = false;
	bool escaped = false;
	std::string currentKey;
	bool collectingKey = false;

	for (size_t i = 0; i < json.size(); ++i)
	{
		char c = json[i];

		if (escaped)
		{
			escaped = false;
			continue;
		}
		if (c == '\\' && inString)
		{
			escaped = true;
			continue;
		}

		if (c == '"')
		{
			if (!inString)
			{
				// Opening quote — start collecting if we're at top level
				inString = true;
				if (depth == 1)
				{
					collectingKey = true;
					currentKey.clear();
				}
			}
			else
			{
				// Closing quote
				inString = false;
				if (depth == 1 && collectingKey)
				{
					// Peek ahead past whitespace to see if next char is ':'
					// (key, not value)
					size_t j = i + 1;
					while (j < json.size() && json[j] == ' ')
						++j;
					if (j < json.size() && json[j] == ':')
						keys.push_back(currentKey);
					collectingKey = false;
				}
			}
			continue;
		}

		if (inString)
		{
			if (collectingKey)
				currentKey += c;
			continue;
		}

		if (c == '{' || c == '[')
			depth++;
		else if (c == '}' || c == ']')
			depth--;
	}
	return keys;
}

void TelemetryManager::privilegedAggregationCycle()
{
	try
	{
		std::string password = AppState::configManager.getConfig<std::string>("firebase-password", "");
		if (password.empty() || password == "69")
			return;

		// firebase-secret-long-string is the Firebase Database Secret (from
		// Project Settings → Service Accounts → Database secrets). It is used
		// as ?auth= on REST calls to bypass security rules for admin
		// reads/writes. firebase-password is the separate custom secret
		// embedded in write payloads.
		std::string dbSecret = AppState::configManager.getConfig<std::string>("firebase-secret-long-string", "");
		if (dbSecret.empty())
		{
			Logger::log("[Telemetry] Aggregation aborted: "
						"firebase-secret-long-string not set in config.");
			return;
		}
		const std::string authParam = "?auth=" + dbSecret;

		std::map<std::string, std::string> headers = {{"Content-Type", "application/json"}};

		Logger::log("[Telemetry] Privileged aggregation cycle starting");

		// ── 1a. Read previous accumulated total from the count summary node ─
		auto prevCountResp = NativeNet::httpGet(FIREBASE_HOST, "/telemetry/users/count.json" + authParam, true);
		int previousTotal = 0;
		if (prevCountResp.second >= 200 && prevCountResp.second < 300)
		{
			previousTotal = parseCountFromSummary(prevCountResp.first, "Total Users: ");
			Logger::log("[Telemetry] Previous accumulated total: " + std::to_string(previousTotal));
		}

		// ── 1b. Read current users node and count only new real entries ──────
		auto usersResp = NativeNet::httpGet(FIREBASE_HOST, "/telemetry/users.json" + authParam, true);
		if (usersResp.second < 200 || usersResp.second >= 300)
		{
			Logger::log("[Telemetry] Aggregation aborted: could not read "
						"telemetry/users, status: " +
						std::to_string(usersResp.second));
			return;
		}
		// Count only real UUID entries — exclude the "count" summary key by
		// name so we're never wrong whether count exists yet or not.
		auto userKeys = extractTopLevelKeys(usersResp.first);
		int newUsers = 0;
		for (const auto &k : userKeys)
		{
			if (k != "count")
				newUsers++;
		}
		int totalUsers = previousTotal + newUsers;
		Logger::log("[Telemetry] New users this cycle: " + std::to_string(newUsers) +
					", total: " + std::to_string(totalUsers));

		// ── 2. Compute current weekId (Monday YYYY-MM-DD) ─────────────────
		auto now = std::chrono::system_clock::now();
		std::time_t nowC = std::chrono::system_clock::to_time_t(now);
		std::tm local_tm = {};
#ifdef _WIN32
		localtime_s(&local_tm, &nowC);
#else
		localtime_r(&nowC, &local_tm);
#endif
		int daysSinceMonday = (local_tm.tm_wday + 6) % 7;
		std::time_t mondayTime = nowC - daysSinceMonday * 86400;
		std::tm mondayTm = {};
#ifdef _WIN32
		localtime_s(&mondayTm, &mondayTime);
#else
		localtime_r(&mondayTime, &mondayTm);
#endif
		char mondayBuffer[16];
		std::strftime(mondayBuffer, sizeof(mondayBuffer), "%Y-%m-%d", &mondayTm);
		std::string weekId(mondayBuffer);

		// ── 3a. Read previous accumulated weekly active from the summary node
		auto prevActiveResp = NativeNet::httpGet(
			FIREBASE_HOST, "/telemetry/weekly_active/" + weekId + "/active_users.json" + authParam, true);
		int previousActive = 0;
		if (prevActiveResp.second >= 200 && prevActiveResp.second < 300)
		{
			previousActive = parseCountFromSummary(prevActiveResp.first, "Active Users: ");
			Logger::log("[Telemetry] Previous accumulated weekly active: " + std::to_string(previousActive));
		}

		// ── 3b. Read current week node and count only new real entries ───────
		auto weekResp =
			NativeNet::httpGet(FIREBASE_HOST, "/telemetry/weekly_active/" + weekId + ".json" + authParam, true);
		if (weekResp.second < 200 || weekResp.second >= 300)
		{
			Logger::log("[Telemetry] Aggregation aborted: could not read "
						"weekly_active, status: " +
						std::to_string(weekResp.second));
			return;
		}
		// Count only real UUID entries — exclude "active_users" summary key by
		// name.
		auto weekKeys = extractTopLevelKeys(weekResp.first);
		int newActive = 0;
		for (const auto &k : weekKeys)
		{
			if (k != "active_users")
				newActive++;
		}
		int weeklyActive = previousActive + newActive;
		Logger::log("[Telemetry] New active this cycle: " + std::to_string(newActive) +
					", total: " + std::to_string(weeklyActive));

		// Both reads succeeded — safe to proceed with delete + write.

		// ── 6. Write accumulated summary: Total Users ──────────────────────
		// Plain string — ?auth=dbSecret handles auth, no secret field needed.
		std::string totalBody = "\"Total Users: " + std::to_string(totalUsers) + "\"";
		auto putTotal =
			NativeNet::httpPut(FIREBASE_HOST, "/telemetry/users/count.json" + authParam, totalBody, true, headers);
		if (putTotal.second >= 200 && putTotal.second < 300)
		{
			Logger::log("[Telemetry] Wrote total users summary: Total Users: " + std::to_string(totalUsers));

			// ── 6b. Delete individual user hash nodes ─────────────────────
			// Reuse userKeys already extracted above — skip "count".
			for (const auto &key : userKeys)
			{
				if (key == "count")
					continue; // never delete the summary node
				std::string delPath = "/telemetry/users/" + key + ".json" + authParam;
				auto delResp = NativeNet::httpDelete(FIREBASE_HOST, delPath, true);
				if (delResp.second >= 200 && delResp.second < 300)
					Logger::log("[Telemetry] Deleted user hash: " + key);
				else
					Logger::log("[Telemetry] Failed to delete user hash: " + key +
								", status: " + std::to_string(delResp.second));
			}
		}
		else
		{
			Logger::log("[Telemetry] Failed to write total users summary, status: " + std::to_string(putTotal.second));
		}

		// ── 7. Write accumulated summary: Active Users ─────────────────────
		// Plain string — ?auth=dbSecret handles auth, no secret field needed.
		std::string activeBody = "\"Active Users: " + std::to_string(weeklyActive) + "\"";
		auto putActive =
			NativeNet::httpPut(FIREBASE_HOST, "/telemetry/weekly_active/" + weekId + "/active_users.json" + authParam,
							   activeBody, true, headers);
		if (putActive.second >= 200 && putActive.second < 300)
		{
			Logger::log("[Telemetry] Wrote weekly active summary: Active Users: " + std::to_string(weeklyActive));

			// ── 7b. Delete individual weekly-active hash nodes ────────────
			// Reuse weekKeys already extracted above — skip "active_users".
			for (const auto &key : weekKeys)
			{
				if (key == "active_users")
					continue; // never delete the summary node
				std::string delPath = "/telemetry/weekly_active/" + weekId + "/" + key + ".json" + authParam;
				auto delResp = NativeNet::httpDelete(FIREBASE_HOST, delPath, true);
				if (delResp.second >= 200 && delResp.second < 300)
					Logger::log("[Telemetry] Deleted weekly-active hash: " + key);
				else
					Logger::log("[Telemetry] Failed to delete weekly-active hash: " + key +
								", status: " + std::to_string(delResp.second));
			}
		}
		else
		{
			Logger::log("[Telemetry] Failed to write weekly active summary, status: " +
						std::to_string(putActive.second));
		}

		Logger::log("[Telemetry] Privileged aggregation cycle complete");
	}
	catch (const std::exception &e)
	{
		Logger::log("[Telemetry] Error in privilegedAggregationCycle: " + std::string(e.what()));
	}
	catch (...)
	{
		Logger::log("[Telemetry] Unknown error in privilegedAggregationCycle");
	}
}
