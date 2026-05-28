#include "window_E.hpp"
#include "windowBackendRegistery.hpp"
#include "appState.hpp"

#include <mutex>
#include <string>
#include <vector>
#include <functional>
std::string getCurrentWindow_E()
{
    std::lock_guard lock(AppState::stateMutex);
    return AppState::state.currentWindow;
}

std::string getCurrentTitle_E()
{
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