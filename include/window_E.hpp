#pragma once
#include <string>
#include <mutex>
#include <string>
#include <vector>
#include <functional>
std::string getCurrentWindow_E();
std::string getCurrentTitle_E();
void registerBackend_E(std::string name, 
    std::function<bool(const std::string&)> matchesEnvironment,
    std::function<void()> initialize,
    std::function<bool()> isUsable,
    std::function<std::string()> getCurrentWindow,
    std::function<std::string()> getCurrentTitle);