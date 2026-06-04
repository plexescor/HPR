#include "window_E.hpp"
#include "windowBackendRegistery.hpp"
#include "appState.hpp"
#include "extensionManager.hpp"

#include <mutex>
#include <string>
#include <vector>
#include <functional>
std::string getCurrentWindow_E()
{
    if (AppState::extManager)
    {
        auto res = AppState::extManager->dispatchOverride("getCurrentWindow_E", {});
        if (res.has_value() && res->type == CppValue::Type::String)
        {
            return res->str_val;
        }
    }
    std::lock_guard lock(AppState::stateMutex);
    return AppState::state.currentWindow;
}

std::string getCurrentTitle_E()
{
    if (AppState::extManager)
    {
        auto res = AppState::extManager->dispatchOverride("getCurrentTitle_E", {});
        if (res.has_value() && res->type == CppValue::Type::String)
        {
            return res->str_val;
        }
    }
    std::lock_guard lock(AppState::stateMutex);
    return AppState::state.currentTitle;
}

void registerBackend_E(std::string name, 
    std::function<bool(const std::string&)> matchesEnvironment,
    std::function<void()> initialize,
    std::function<bool()> isUsable,
    std::function<std::string()> getCurrentWindow,
    std::function<std::string()> getCurrentTitle)
{
    registerBackend
    ({
        name,
        matchesEnvironment,
        initialize,
        isUsable,
        getCurrentWindow,
        getCurrentTitle
    });
}

std::map<std::string, uint64_t> getLiveTimeLogPerApp_E()
{
    if (AppState::extManager)
    {
        auto res = AppState::extManager->dispatchOverride("getLiveTimeLogPerApp_E", {});
        if (res.has_value() && res->type == CppValue::Type::Struct)
        {
            std::map<std::string, uint64_t> results;
            for (const auto& [k, v] : res->struct_val)
            {
                if (v.type == CppValue::Type::Double)
                {
                    results[k] = static_cast<uint64_t>(v.double_val);
                }
            }
            return results;
        }
    }
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    return AppState::state.timeLog_PerApp;
}

std::map<std::string, uint64_t> getLiveTimeLogPerTab_E()
{
    if (AppState::extManager)
    {
        auto res = AppState::extManager->dispatchOverride("getLiveTimeLogPerTab_E", {});
        if (res.has_value() && res->type == CppValue::Type::Struct)
        {
            std::map<std::string, uint64_t> results;
            for (const auto& [k, v] : res->struct_val)
            {
                if (v.type == CppValue::Type::Double)
                {
                    results[k] = static_cast<uint64_t>(v.double_val);
                }
            }
            return results;
        }
    }
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    return AppState::state.timeLog_PerTab;
}

std::map<std::string, uint64_t> getLiveTimeLogPerProject_E()
{
    if (AppState::extManager)
    {
        auto res = AppState::extManager->dispatchOverride("getLiveTimeLogPerProject_E", {});
        if (res.has_value() && res->type == CppValue::Type::Struct)
        {
            std::map<std::string, uint64_t> results;
            for (const auto& [k, v] : res->struct_val)
            {
                if (v.type == CppValue::Type::Double)
                {
                    results[k] = static_cast<uint64_t>(v.double_val);
                }
            }
            return results;
        }
    }
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    return AppState::state.timeLog_PerProject;
}