#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

#include "configManager.hpp"

ConfigManager::ConfigManager() { loadConfig(); }

void ConfigManager::loadConfig()
{
    std::filesystem::path exePath;
    #ifdef _WIN32
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        exePath = std::filesystem::path(buffer);

    #elif defined(__linux__)
        exePath = std::filesystem::read_symlink("/proc/self/exe");
    #else
        exePath = std::filesystem::current_path() / "fallback";
    #endif

    csvPath = exePath.parent_path() / fileName; // store it

    std::ifstream file(csvPath);
    if (!file.is_open())
    {
        std::cerr << "Warning: config.csv not found at " << csvPath
                  << ". App will use default settings.\n";
        return;
    }

    std::string line;

    //One line at a time
    while (std::getline(file, line))
    {
        //if empty or starts with #, continue, write comments with #
        if (line.empty() || line.starts_with("#")) continue;

        //find comma pos
        size_t commaPos = line.find(',');

        //If it has a comma then do this
        if (commaPos != std::string::npos)
        {
            config.push_back({line.substr(0, commaPos), line.substr(commaPos + 1)});
        }
    }
}

bool ConfigManager::getConfig(const std::string &requestedParam, bool defaultValue)
{
    for (const auto &[param, value] : config)
    {
        if (requestedParam.contains(param))
        {
            if (value == "true") return true;
            else if (value == "false") return false;
        }
    }
    return defaultValue;
}