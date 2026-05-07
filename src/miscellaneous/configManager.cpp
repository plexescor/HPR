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

    std::string tempPath;
    #ifdef _WIN32
        tempPath = std::getenv("APPDATA");
        tempPath += "/HPR/HPR_Config/";
    #else
        const char* home = std::getenv("HOME");
        if (!home) throw std::runtime_error("HOME env var not set");
        tempPath = home;
        tempPath += "/.config/HPR/";
    #endif
    
    std::filesystem::create_directories(tempPath);
    filePath = tempPath + fileName;

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Warning: config.csv not found at " << filePath
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