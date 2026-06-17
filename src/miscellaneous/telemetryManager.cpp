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

            auto response = NativeNet::httpPost(FIREBASE_HOST, "/telemetry/users.json", body, true, headers);
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

                std::string path = "/telemetry/weekly_active/" + week_id + ".json";
                auto response = NativeNet::httpPost(FIREBASE_HOST, path, body, true, headers);
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
