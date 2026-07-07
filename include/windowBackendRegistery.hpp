#pragma once

#include <vector>
#include <functional>
#include <string>

struct WindowBackend
{
    std::string name;

    std::function<bool(const std::string& desktopEnvironment)> matchesEnvironment;    
    std::function<void()> initialize;
        
    std::function<std::string()> getCurrentWindow;
    std::function<std::string()> getCurrentTitle;
    std::function<std::string()> getCurrentPid;
};

extern std::vector<WindowBackend> registeredBackends;

void registerBackend(const WindowBackend& backend);

WindowBackend* getBackendByName(const std::string& name);