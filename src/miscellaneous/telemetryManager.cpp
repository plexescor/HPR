#include "telemetryManager.hpp"
#include "appState.hpp"
#include "appEvents.hpp"
#include "netUtilities.hpp"
#include "logger.hpp"
#include "timeUtils.hpp"

#include <filesystem>
#include <chrono>
#include <thread>
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <ctime>
#include <cstring>
#include <map>

namespace {
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
    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4"; // UUID version 4
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);
    return ss.str();
}

void TelemetryManager::init()
{
    // Run the telemetry check asynchronously in a background thread
    std::thread([]() {
        // Wait 5 seconds after startup for database initialization to finish
        std::this_thread::sleep_for(std::chrono::seconds(5));
        checkAndSend();
    }).detach();

    // Subscribe to midnight rollover to re-evaluate telemetry when the day transitions
    EventHub::connect(Event::MIDNIGHT_ROLLOVER, [](EventData data) {
        std::thread([]() {
            // Wait 5 seconds after rollover for the database rollover to fully settle
            std::this_thread::sleep_for(std::chrono::seconds(5));
            checkAndSend();
        }).detach();
    });

    // Privileged aggregation loop — runs immediately on startup, then every 30 minutes.
    // Only active when firebase-password is set to a non-default value.
    std::thread([]() {
        // Short delay to let app and network settle before first run
        std::this_thread::sleep_for(std::chrono::seconds(8));
        while (true) {
            std::string pw = AppState::configManager.getConfig<std::string>("firebase-password", "69");
            if (!pw.empty() && pw != "69") {
                privilegedAggregationCycle();
            }
            std::this_thread::sleep_for(std::chrono::minutes(30));
        }
    }).detach();
}

void TelemetryManager::checkAndSend()
{
    try {
        // Bail out if user hasn't opted in (default is false)
        bool enabled = AppState::configManager.getConfig<bool>("anonymous-telemetry", false);
        if (!enabled) {
            return;
        }

        // Get or generate UUID
        std::string userId = AppState::configManager.getConfig<std::string>("user-id", "");
        if (userId.empty()) {
            userId = generateUUID();
            AppState::configManager.setConfig("user-id", userId);
        }

        // 1. Report User Registration (Total User Count)
        // Only send on the very first launch — isFirstLaunch() is true when HPR.lock
        // was created this session (i.e. it didn't exist before this run).
        if (AppState::configManager.isFirstLaunch()) {
            auto now = std::chrono::system_clock::now();
            uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

            std::string body = "{\"userId\":\"" + userId + "\",\"registeredAt\":" + std::to_string(ts) + "}";
            std::map<std::string, std::string> headers = {
                {"Content-Type", "application/json"}
            };

            // PUT with userId as key → same user can never produce duplicate entries
            std::string regPath = "/telemetry/users/" + userId + ".json";
            auto response = NativeNet::httpPut(FIREBASE_HOST, regPath, body, true, headers);
            if (response.second >= 200 && response.second < 300) {
                Logger::log("[Telemetry] User registered successfully");
            } else {
                Logger::log("[Telemetry] User registration failed with code: " + std::to_string(response.second));
            }
        }

        // 2. Report Active Usage (4+ days/week)
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm = {};
#ifdef _WIN32
        localtime_s(&local_tm, &now_c);
#else
        localtime_r(&now_c, &local_tm);
#endif

        int current_day_of_week = local_tm.tm_wday; // 0 = Sunday, 1 = Monday, ..., 6 = Saturday
        int days_since_monday = (current_day_of_week + 6) % 7;

        // Calculate Monday date string for this week (YYYY-MM-DD)
        std::time_t monday_time = now_c - days_since_monday * 86400;
        std::tm monday_tm = {};
#ifdef _WIN32
        localtime_s(&monday_tm, &monday_time);
#else
        localtime_r(&monday_time, &monday_tm);
#endif
        char monday_buf[16];
        std::strftime(monday_buf, sizeof(monday_buf), "%Y-%m-%d", &monday_tm);
        std::string week_id(monday_buf);

        // Check active days by verifying the existence of database files
        int active_days_count = 0;
        for (int i = 0; i < 7; ++i) {
            std::time_t day_time = now_c + (i - days_since_monday) * 86400;
            std::tm day_tm = {};
#ifdef _WIN32
            localtime_s(&day_tm, &day_time);
#else
            localtime_r(&day_time, &day_tm);
#endif
            char buf[16];
            std::strftime(buf, sizeof(buf), "%d-%m-%y", &day_tm);
            std::string date_str(buf);

            std::string db_path;
            std::string mm_yy = extractMMYY_from_DDMMYY(date_str);
            if (mm_yy.empty()) continue;

#ifdef _WIN32
            char* appData = std::getenv("APPDATA");
            if (appData) {
                db_path = std::string(appData) + "/HPR/HPR_DB/" + mm_yy + "/" + date_str + ".db";
            }
#else
            const char* home = std::getenv("HOME");
            if (home) {
                db_path = std::string(home) + "/.local/share/HPR/HPR_DB/" + mm_yy + "/" + date_str + ".db";
            }
#endif

            if (!db_path.empty() && std::filesystem::exists(db_path)) {
                active_days_count++;
            }
        }

        if (active_days_count >= 4) {
            std::string lastReportedWeek = AppState::configManager.getConfig<std::string>("last-reported-week", "");
            if (lastReportedWeek != week_id) {
                std::string body = "{\"userId\":\"" + userId + "\"}";
                std::map<std::string, std::string> headers = {
                    {"Content-Type", "application/json"}
                };

                // PUT with userId as key under week_id → same user same week = same path = no duplicate
                std::string path = "/telemetry/weekly_active/" + week_id + "/" + userId + ".json";
                auto response = NativeNet::httpPut(FIREBASE_HOST, path, body, true, headers);
                if (response.second >= 200 && response.second < 300) {
                    AppState::configManager.setConfig("last-reported-week", week_id);
                    Logger::log("[Telemetry] Weekly active reported successfully for week: " + week_id);
                } else {
                    Logger::log("[Telemetry] Weekly active reporting failed with code: " + std::to_string(response.second));
                }
            }
        }

    } catch (const std::exception& e) {
        Logger::log("[Telemetry] Error in checkAndSend: " + std::string(e.what()));
    } catch (...) {
        Logger::log("[Telemetry] Unknown error in checkAndSend");
    }
}

int TelemetryManager::countJsonTopLevelKeys(const std::string& json)
{
    // Counts the number of top-level keys in a flat Firebase JSON object like
    // {"key1":{...},"key2":"val",...}. Uses depth tracking — only counts
    // key-colon patterns at depth 1 (directly inside the outer braces).
    if (json.empty() || json == "null") return 0;

    int count = 0;
    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (size_t i = 0; i < json.size(); ++i)
    {
        char c = json[i];

        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\' && inString) {
            escaped = true;
            continue;
        }
        if (c == '"') {
            inString = !inString;
            continue;
        }
        if (inString) continue;

        if (c == '{' || c == '[') { depth++; continue; }
        if (c == '}' || c == ']') { depth--; continue; }

        // A ':' at depth 1 means we're seeing a top-level key-value separator
        if (c == ':' && depth == 1) {
            count++;
        }
    }
    return count;
}

bool TelemetryManager::jsonHasTopLevelKey(const std::string& json, const std::string& key)
{
    // Checks whether a specific key exists at the top level of a Firebase JSON object.
    // Searches for the pattern "key": at depth 1 to avoid false matches inside nested values.
    if (json.empty() || json == "null") return false;

    const std::string target = "\"" + key + "\"";
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    size_t i = 0;

    while (i < json.size()) {
        char c = json[i];

        if (escaped) { escaped = false; ++i; continue; }
        if (c == '\\' && inString) { escaped = true; ++i; continue; }
        if (c == '"') {
            inString = !inString;
            if (inString && depth == 1) {
                // Check if this quoted string matches the target key
                if (json.compare(i, target.size(), target) == 0) {
                    // Make sure the next non-space char after closing quote is ':'
                    size_t j = i + target.size();
                    while (j < json.size() && json[j] == ' ') ++j;
                    if (j < json.size() && json[j] == ':') return true;
                }
            }
            ++i;
            continue;
        }
        if (inString) { ++i; continue; }
        if (c == '{' || c == '[') { depth++; }
        else if (c == '}' || c == ']') { depth--; }
        ++i;
    }
    return false;
}

int TelemetryManager::parseCountFromSummary(const std::string& json, const std::string& prefix)
{
    if (json.empty() || json == "null") return 0;

    // Extract the "value" field from:
    // {"value":"Total Users: 42","secret":"..."}
    const std::string key = "\"value\":\"";
    size_t start = json.find(key);
    if (start == std::string::npos) return 0;

    start += key.size();
    size_t end = json.find('"', start);
    if (end == std::string::npos) return 0;

    std::string value = json.substr(start, end - start);

    auto pos = value.find(prefix);
    if (pos == std::string::npos) return 0;

    try {
        return std::stoi(value.substr(pos + prefix.size()));
    } catch (...) {
        return 0;
    }
}

void TelemetryManager::privilegedAggregationCycle()
{
    try {
        std::string password1 = AppState::configManager.getConfig<std::string>("firebase-password", "69");
        if (password1.empty() || password1 == "69") return;

        std::map<std::string, std::string> headers = {{"Content-Type", "application/json"}};

        Logger::log("[Telemetry] Privileged aggregation cycle starting");

        // ── 0. Unlock: write password to admin_secret ──────────────────────
        std::string secretBody = "\"" + password1 + "\"";
        auto secretResp = NativeNet::httpPut(FIREBASE_HOST, "/telemetry/admin_secret.json",
                                             secretBody, true, headers);
        if (secretResp.second == 403 || secretResp.second == 401) {
            Logger::log("[Telemetry] Aggregation aborted: incorrect firebase-password. No data was modified.");
            return;
        }
        if (secretResp.second < 200 || secretResp.second >= 300) {
            Logger::log("[Telemetry] Aggregation aborted: could not write admin_secret, status: "
                        + std::to_string(secretResp.second));
            return;
        }

        // ── 1a. Read previous accumulated total from the count summary node ─
        auto prevCountResp = NativeNet::httpGet(FIREBASE_HOST, "/telemetry/users/count.json", true);
        int previousTotal = 0;
        if (prevCountResp.second >= 200 && prevCountResp.second < 300) {
            previousTotal = parseCountFromSummary(prevCountResp.first, "Total Users: ");
            Logger::log("[Telemetry] Previous accumulated total: " + std::to_string(previousTotal));
        }

        // ── 1b. Read current users node and count only new real entries ──────
        auto usersResp = NativeNet::httpGet(FIREBASE_HOST, "/telemetry/users.json", true);
        if (usersResp.second < 200 || usersResp.second >= 300) {
            Logger::log("[Telemetry] Aggregation aborted: could not read telemetry/users, status: "
                        + std::to_string(usersResp.second));
            return;
        }
        int newUsers = countJsonTopLevelKeys(usersResp.first);
        newUsers = std::max(0, newUsers - 1);
        int totalUsers = previousTotal + newUsers;
        Logger::log("[Telemetry] New users this cycle: " + std::to_string(newUsers)
                    + ", total: " + std::to_string(totalUsers));

        // ── 2. Compute current week_id (Monday YYYY-MM-DD) ─────────────────
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm = {};
#ifdef _WIN32
        localtime_s(&local_tm, &now_c);
#else
        localtime_r(&now_c, &local_tm);
#endif
        int days_since_monday = (local_tm.tm_wday + 6) % 7;
        std::time_t monday_time = now_c - days_since_monday * 86400;
        std::tm monday_tm = {};
#ifdef _WIN32
        localtime_s(&monday_tm, &monday_time);
#else
        localtime_r(&monday_time, &monday_tm);
#endif
        char monday_buf[16];
        std::strftime(monday_buf, sizeof(monday_buf), "%Y-%m-%d", &monday_tm);
        std::string week_id(monday_buf);

        // ── 3a. Read previous accumulated weekly active from the summary node 
        auto prevActiveResp = NativeNet::httpGet(FIREBASE_HOST,
            "/telemetry/weekly_active/" + week_id + "/active_users.json", true);
        int previousActive = 0;
        if (prevActiveResp.second >= 200 && prevActiveResp.second < 300) {
            previousActive = parseCountFromSummary(prevActiveResp.first, "Active Users: ");
            Logger::log("[Telemetry] Previous accumulated weekly active: " + std::to_string(previousActive));
        }

        // ── 3b. Read current week node and count only new real entries ───────
        auto weekResp = NativeNet::httpGet(FIREBASE_HOST,
            "/telemetry/weekly_active/" + week_id + ".json", true);
        if (weekResp.second < 200 || weekResp.second >= 300) {
            Logger::log("[Telemetry] Aggregation aborted: could not read weekly_active, status: "
                        + std::to_string(weekResp.second));
            return;
        }
        int newActive = countJsonTopLevelKeys(weekResp.first);
        newActive = std::max(0, newActive - 1);
        int weeklyActive = previousActive + newActive;
        Logger::log("[Telemetry] New active this cycle: " + std::to_string(newActive)
                    + ", total: " + std::to_string(weeklyActive));

        // Both reads succeeded — safe to proceed with delete + write.


        // ── 6. Write accumulated summary: Total Users ──────────────────────
        std::string password2 = AppState::configManager.getConfig<std::string>("firebase-password", "");

        std::string totalBody =
            "{\"value\":\"Total Users: " + std::to_string(totalUsers) +
            "\",\"secret\":\"" + password2 + "\"}";
        auto putTotal = NativeNet::httpPut(FIREBASE_HOST,
            "/telemetry/users/count.json", totalBody, true, headers);
        if (putTotal.second >= 200 && putTotal.second < 300) {
            Logger::log("[Telemetry] Wrote total users summary: Total Users: " + std::to_string(totalUsers));
        } else {
            Logger::log("[Telemetry] Failed to write total users summary, status: " + std::to_string(putTotal.second));
        }

        // ── 7. Write accumulated summary: Active Users ─────────────────────
        std::string password = AppState::configManager.getConfig<std::string>("firebase-password", "");

        std::string activeBody =
            "{\"value\":\"Active Users: " + std::to_string(weeklyActive) +
            "\",\"secret\":\"" + password + "\"}";
        auto putActive = NativeNet::httpPut(FIREBASE_HOST,
            "/telemetry/weekly_active/" + week_id + "/active_users.json",
            activeBody, true, headers);
        if (putActive.second >= 200 && putActive.second < 300) {
            Logger::log("[Telemetry] Wrote weekly active summary: Active Users: " + std::to_string(weeklyActive));
        } else {
            Logger::log("[Telemetry] Failed to write weekly active summary, status: " + std::to_string(putActive.second));
        }

        Logger::log("[Telemetry] Privileged aggregation cycle complete");

    } catch (const std::exception& e) {
        Logger::log("[Telemetry] Error in privilegedAggregationCycle: " + std::string(e.what()));
    } catch (...) {
        Logger::log("[Telemetry] Unknown error in privilegedAggregationCycle");
    }
}

