#pragma once
#include <string>
#include <mutex>
#include <vector>
#include <functional>
#include <map>
std::string getCurrentWindow_E();
std::string getCurrentTitle_E();
std::map<std::string, long> getLiveTimeLogPerApp_E();
std::map<std::string, long> getLiveTimeLogPerTab_E();
std::map<std::string, long> getLiveTimeLogPerProject_E();
void registerBackend_E(std::string name, 
    std::function<bool(const std::string&)> matchesEnvironment,
    std::function<void()> initialize,
    std::function<bool()> isUsable,
    std::function<std::string()> getCurrentWindow,
    std::function<std::string()> getCurrentTitle);