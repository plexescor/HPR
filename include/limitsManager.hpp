#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <map>

class LimitsManager
{
public:
    LimitsManager();
    ~LimitsManager();

    void run();

    static void setLimit(const std::string& appName, int minutes);
    static void setGoal(const std::string& appName, int minutes);

    static std::string getLimitRemaining(const std::string& appName, uint64_t currentDurationMs, int limitMins);
    static std::string getGoalRemaining(const std::string& appName, uint64_t currentDurationMs, int goalMins);

private:
    void checkLoop();

private:
    std::atomic<bool> running{false};
    std::thread checkerThread;

    // Track which notifications have already been sent today
    // Maps raw app names to notification flags
    static std::map<std::string, bool> limitWarningSent;
    static std::map<std::string, bool> limitReachedSent;
    static std::map<std::string, bool> goalWarningSent;
    static std::map<std::string, bool> goalReachedSent;

    static std::map<std::string, uint64_t> limitTimeBase;
    static std::map<std::string, uint64_t> goalTimeBase;
    static std::map<std::string, bool> killSent;
    static std::chrono::steady_clock::time_point lastGlobalKillTime;

    static std::mutex limitsMutex;
};
