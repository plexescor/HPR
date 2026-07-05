#include "limitsManager.hpp"
#include "appState.hpp"
#include "windowUtilities.hpp"
#include "logger.hpp"
#include <chrono>
#include "timeUtils.hpp"
#include "configManager.hpp"
#include "appEvents.hpp"
#include "extensionManager.hpp"

//i dont remember why i defined these as globals
//but there must be a reason so keep it as is
std::map<std::string, bool> LimitsManager::limitWarningSent;
std::map<std::string, bool> LimitsManager::limitReachedSent;
std::map<std::string, bool> LimitsManager::goalWarningSent;
std::map<std::string, bool> LimitsManager::goalReachedSent;
std::map<std::string, bool> LimitsManager::killSent;
std::chrono::steady_clock::time_point LimitsManager::lastGlobalKillTime;
std::mutex LimitsManager::limitsMutex;

LimitsManager::LimitsManager() 
{
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    for (const auto& [appName, _] : AppState::state.appLimits) {
        if (!AppState::state.limitTimeBase.count(appName)) {
            // only set if not already loaded from DB
            if (AppState::state.timeLog_PerApp.count(appName))
                AppState::state.limitTimeBase[appName] = AppState::state.timeLog_PerApp[appName];
        }
    }
    for (const auto& [appName, _] : AppState::state.appGoals) {
        if (!AppState::state.goalTimeBase.count(appName)) {
            if (AppState::state.timeLog_PerApp.count(appName))
                AppState::state.goalTimeBase[appName] = AppState::state.timeLog_PerApp[appName];
        }
    }
}


LimitsManager::~LimitsManager()
{
    running = false;
    if (checkerThread.joinable())
    {
        checkerThread.join();
    }
}

void LimitsManager::run()
{
    running = true;
    checkerThread = std::thread(&LimitsManager::checkLoop, this);
}

void LimitsManager::limitReached(const std::string& appName)
{
    //hook
    //so sorry it was a false positive that it existed
    if (AppState::extManager)
    {
        auto res = AppState::extManager->dispatchOverride("limitReached", { CppValue(CppValue::Type::String, appName) });
        if (res.has_value() && res->type == CppValue::Type::String)
        {
            return;
        }
    }

    std::string pid;
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        if (AppState::state.appNamePid.count(appName))
            pid = AppState::state.appNamePid.at(appName);
    }
    std::string cmd;
    if (!pid.empty())
    {
        #ifdef _WIN32
            cmd = "taskkill /F /PID " + pid;
        #else
            cmd = "kill -9 " + pid + " 2>/dev/null";
        #endif
    }
    else
    {
        #ifdef _WIN32
            cmd = "taskkill /F /IM " + appName + " /IM " + appName + ".exe";
        #else
            cmd = "pkill -f \"" + appName + "\" || killall \"" + appName + "\"";
        #endif
    }
    runSystemCommand_UNSAFE(cmd);
}

void LimitsManager::setLimit(const std::string& appName, int minutes)
{
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        if (minutes <= 0) 
        {
            AppState::state.appLimits.erase(appName);
            AppState::state.limitTimeBase.erase(appName);
        } 
        else 
        {
            AppState::state.appLimits[appName] = minutes;
            AppState::state.limitTimeBase[appName] = 
                AppState::state.timeLog_PerApp.count(appName) 
                ? AppState::state.timeLog_PerApp[appName] : 0;
        }
    }
    
    // Reset flags so they can trigger again
    std::lock_guard<std::mutex> lock(limitsMutex);
    limitWarningSent[appName] = false;
    limitReachedSent[appName] = false;
    killSent[appName] = false;
    
    Logger::log("[LimitsManager] Set limit for " + appName + " to " + std::to_string(minutes) + " minutes");
}


void LimitsManager::setGoal(const std::string& appName, int minutes)
{
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        if (minutes <= 0)
        {
            AppState::state.appGoals.erase(appName);
            AppState::state.goalTimeBase.erase(appName);
        }
        else
        {
            AppState::state.appGoals[appName] = minutes;
            AppState::state.goalTimeBase[appName] = 
                AppState::state.timeLog_PerApp.count(appName)
                ? AppState::state.timeLog_PerApp[appName] : 0;
        }
    }
    
    std::lock_guard<std::mutex> lock(limitsMutex);
    goalWarningSent[appName] = false;
    goalReachedSent[appName] = false;

    Logger::log("[LimitsManager] Set goal for " + appName + " to " + std::to_string(minutes) + " minutes");
}


void LimitsManager::checkLoop()
{
    while (running)
    {
        std::map<std::string, uint64_t> currentUsage;
        std::map<std::string, int> activeLimits;
        std::map<std::string, int> activeGoals;
        std::map<std::string, uint64_t> limitBases;
        std::map<std::string, uint64_t> goalBases;

        {
            std::lock_guard<std::mutex> lock(AppState::stateMutex);
            currentUsage = AppState::state.timeLog_PerApp;
            activeLimits = AppState::state.appLimits;
            activeGoals = AppState::state.appGoals;
            limitBases = AppState::state.limitTimeBase;
            goalBases = AppState::state.goalTimeBase;
        }

        // Check Limits
        for (const auto& [appName, minutes] : activeLimits)
        {
            if (minutes <= 0) continue;

            uint64_t limitMs = static_cast<uint64_t>(minutes) * 60 * 1000;
            uint64_t warningMs = static_cast<uint64_t>(limitMs * 0.9);

            if (currentUsage.count(appName))
            {
                uint64_t duration = currentUsage.at(appName);
                uint64_t base = limitBases.count(appName) ? limitBases.at(appName) : 0;
                uint64_t elapsed = (duration >= base) ? (duration - base) : 0;

                bool sendWarning = false;
                bool sendReached = false;
                bool doKill = false;

                std::string currentApp;

                {
                    std::lock_guard<std::mutex> lock(AppState::stateMutex);
                    currentApp = AppState::state.currentWindow;
                }

                {
                    std::lock_guard<std::mutex> lock(limitsMutex);

                    if (elapsed >= limitMs)
                    {
                        if (!limitReachedSent[appName])
                        {
                            limitReachedSent[appName] = true;
                            sendReached = true;
                        }

                        uint64_t killMs = static_cast<uint64_t>(limitMs * 1.05);
                        if (elapsed >= killMs && !killSent[appName])
                        {
                            doKill = true;
                            killSent[appName] = true;
                        }
                    }
                    else if (elapsed >= warningMs)
                    {
                        if (!limitWarningSent[appName])
                        {
                            limitWarningSent[appName] = true;
                            sendWarning = true;
                        }
                    }
                }

                // Translate to aliased name for notification display
                std::string aliasedName;
                {
                    std::lock_guard<std::mutex> lock(AppState::stateMutex);
                    aliasedName = AppState::aliasManager.getAlias(appName);
                }

                if (sendReached)
                    showNotification("HPR Alert", "Daily limit reached for " + aliasedName + "!");
                if (doKill)
                {
                    if (AppState::configManager.getConfig("kill-apps", true))
                    {
                        showNotification("HPR Termination", aliasedName + " has exceeded its limit by 5%. Terminating process!");
                        
                        // Check global kill cooldown
                        auto now = std::chrono::steady_clock::now();
                        auto timeSinceLastKill = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastGlobalKillTime).count();
                        int killCooldown = AppState::configManager.getConfig("kill-cooldown", 2500);
                        if (timeSinceLastKill >= killCooldown)
                        {
                            lastGlobalKillTime = now;
                            //otherwise it kills them even if they run in background
                            if (currentApp == appName)
                                limitReached(appName);
                        }
                    }
                }

                if (killSent[appName])
                {
                    if (AppState::configManager.getConfig("kill-apps", true))
                    {
                        // Check global kill cooldown before repeating kill command
                        auto now = std::chrono::steady_clock::now();
                        auto timeSinceLastKill = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastGlobalKillTime).count();
                        int killCooldown = AppState::configManager.getConfig("kill-cooldown", 2500);
                        if (timeSinceLastKill >= killCooldown)
                        {
                            lastGlobalKillTime = now;
                            //otherwise it kills them even if they run in background
                            if (currentApp == appName)
                                limitReached(appName);
                        }
                    }
                }

                if (sendWarning)
                    showNotification("HPR Warning", "You have used 90% of your daily limit for " + aliasedName + "!");
            }
        }

        // Check Goals
        for (const auto& [appName, minutes] : activeGoals)
        {
            if (minutes <= 0) continue;

            uint64_t goalMs = static_cast<uint64_t>(minutes) * 60 * 1000;
            uint64_t warningMs = static_cast<uint64_t>(goalMs * 0.9);

            if (currentUsage.count(appName))
            {
                uint64_t duration = currentUsage.at(appName);
                uint64_t base = goalBases.count(appName) ? goalBases.at(appName) : 0;
                uint64_t elapsed = (duration >= base) ? (duration - base) : 0;

                bool sendGoalReached = false;
                bool sendGoalWarning = false;

                {
                    std::lock_guard<std::mutex> lock(limitsMutex);

                    if (elapsed >= goalMs)
                    {
                        if (!goalReachedSent[appName])
                        {
                            goalReachedSent[appName] = true;
                            sendGoalReached = true;
                        }
                    }
                    else if (elapsed >= warningMs)
                    {
                        if (!goalWarningSent[appName])
                        {
                            goalWarningSent[appName] = true;
                            sendGoalWarning = true;
                        }
                    }
                }

                std::string aliasedName;
                {
                    std::lock_guard<std::mutex> lock(AppState::stateMutex);
                    aliasedName = AppState::aliasManager.getAlias(appName);
                }

                if (sendGoalReached)
                    showNotification("HPR Congratulations", "You have met your goal for " + aliasedName + "!");
                if (sendGoalWarning)
                    showNotification("HPR Goal Alert", "You are close to reaching your goal for " + aliasedName + "!");
            }
        }

        for (int i = 0; i < 6 && running; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

std::string LimitsManager::getLimitRemaining(const std::string& appName, uint64_t currentDurationMs, int limitMins)
{
    if (limitMins <= 0) return "";

    uint64_t limitMs = static_cast<uint64_t>(limitMins) * 60 * 1000;
    uint64_t base = 0;
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        if (AppState::state.limitTimeBase.count(appName))
            base = AppState::state.limitTimeBase.at(appName);
    }
    uint64_t elapsed = (currentDurationMs >= base) ? (currentDurationMs - base) : 0;

    if (elapsed >= limitMs) return "0s";
    uint64_t remaining = limitMs - elapsed;
    return formatTime_HHMMSS(remaining);
}

std::string LimitsManager::getGoalRemaining(const std::string& appName, uint64_t currentDurationMs, int goalMins)
{
    if (goalMins <= 0) return "";

    uint64_t goalMs = static_cast<uint64_t>(goalMins) * 60 * 1000;
    uint64_t base = 0;
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        if (AppState::state.goalTimeBase.count(appName))
            base = AppState::state.goalTimeBase.at(appName);
    }
    uint64_t elapsed = (currentDurationMs >= base) ? (currentDurationMs - base) : 0;

    if (elapsed >= goalMs) return "Met!";
    uint64_t remaining = goalMs - elapsed;
    return formatTime_HHMMSS(remaining);
}
