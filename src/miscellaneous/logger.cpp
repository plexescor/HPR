#include "logger.hpp"
#include <iostream>
#include <fstream>
#include <mutex>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <iomanip>

static std::mutex s_logMutex;

std::string Logger::getLogFilePath()
{
    std::string logDir;

#ifdef _WIN32
    const char* appData = getenv("APPDATA");
    logDir = appData ? std::string(appData) + "\\HPR\\HPR_Config\\logs" : ".\\logs";
#else
    const char* home = getenv("HOME");
    logDir = home ? std::string(home) + "/.config/HPR/logs" : "./logs";
#endif

    std::filesystem::create_directories(logDir);

    // Get current date for filename
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream fileName;
    fileName << std::put_time(&tm, "%Y-%m-%d") << ".log";

#ifdef _WIN32
    return logDir + "\\" + fileName.str();
#else
    return logDir + "/" + fileName.str();
#endif
}

void Logger::log(const std::string message)
{
    std::lock_guard<std::mutex> lock(s_logMutex);

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream entry;
    entry << "[" << std::put_time(&tm, "%H:%M:%S") << "] " << message << "\n";

    std::ofstream file(getLogFilePath(), std::ios::app);
    if (file.is_open())
        file << entry.str();
    else
        std::cerr << "[Logger] Failed to open log file\n";
}