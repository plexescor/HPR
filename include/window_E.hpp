#pragma once
#include <string>
#include <mutex>
#include <vector>
#include <functional>
#include <map>
std::string getCurrentWindow_E();
std::string getCurrentTitle_E();
std::map<std::string, uint64_t> getLiveTimeLogPerApp_E();
std::map<std::string, uint64_t> getLiveTimeLogPerTab_E();
std::map<std::string, uint64_t> getLiveTimeLogPerProject_E();
void registerBackend_E(std::string name, 
    std::function<bool(const std::string&)> matchesEnvironment,
    std::function<void()> initialize,
    std::function<std::string()> getCurrentWindow,
    std::function<std::string()> getCurrentTitle,
    std::function<std::string()> getCurrentPid);