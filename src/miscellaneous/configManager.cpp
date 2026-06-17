#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <type_traits>
#include <cstdlib>
#include "logger.hpp"
#ifdef _WIN32
#include <windows.h>
#endif

#include "configManager.hpp"
#include "autostartManager.hpp"

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

    std::filesystem::path lockPath = std::filesystem::path(tempPath) / "HPR.lock";
    bool lockExists = std::filesystem::exists(lockPath);
    if (!lockExists)
    {
        firstLaunch = true;
        std::ofstream lockFile(lockPath);
        if (lockFile.is_open())
        {
            lockFile << "HPR Installed";
            lockFile.close();
        }
    }
    else
    {
        firstLaunch = false;
    }
    Logger::log("[ConfigManager] First launch: " + std::string(firstLaunch ? "yes" : "no"));

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Warning: config.csv not found at " << filePath
                  << ". App will use default settings.\n";
        Logger::log("Warning: config.csv not found at " + filePath + ". App will use default settings.");
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

void ConfigManager::saveConfig()
{
    std::ofstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Error: Failed to open config file for writing: " << filePath << std::endl;
        Logger::log("Error: Failed to open config file for writing: " + filePath);
        return;
    }
    for (const auto& [param, value] : config)
    {
        file << param << "," << value << "\n";
    }
    file.close();
}

template <typename T>
T ConfigManager::getConfig(const std::string requestedParam, const T& defaultValue)
{
    for (const auto &[param, value] : config)
    {
        if (requestedParam.contains(param))
        {
            if constexpr (std::is_same_v<T, bool>)
            {
                if (value == "true") return true;
                if (value == "false") return false;
                return defaultValue;
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                return value;
            }
            else if constexpr (std::is_arithmetic_v<T>)
            {
                char* endptr = nullptr;
                double val = std::strtod(value.c_str(), &endptr);
                if (endptr != value.c_str())
                {
                    return static_cast<T>(val);
                }
                return defaultValue;
            }
            else
            {
                std::stringstream ss(value);
                T result;
                if (ss >> result)
                {
                    return result;
                }
            }
        }
    }
    return defaultValue;
}

template <typename T>
void ConfigManager::setConfig(const std::string paramName, const T& value)
{
    std::string valueStr;
    if constexpr (std::is_same_v<T, bool>)
    {
        valueStr = value ? "true" : "false";
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        valueStr = value;
    }
    else if constexpr (std::is_same_v<T, const char*>)
    {
        valueStr = std::string(value);
    }
    else if constexpr (std::is_arithmetic_v<T>)
    {
        valueStr = std::to_string(value);
    }
    else
    {
        std::stringstream ss;
        ss << value;
        valueStr = ss.str();
    }

    bool updated = false;
    for (auto& [param, val] : config)
    {
        if (param == paramName)
        {
            val = valueStr;
            updated = true;
            break;
        }
    }
    if (!updated)
    {
        config.push_back({paramName, valueStr});
    }
    saveConfig();

    if (paramName == "autostart" || paramName == "headless-mode")
    {
        bool autostartVal = false;
        bool headlessVal = false;
        for (const auto& [param, val] : config)
        {
            if (param == "autostart") autostartVal = (val == "true");
            if (param == "headless-mode") headlessVal = (val == "true");
        }
        AutostartManager::setEnabled(autostartVal, headlessVal);
    }
}

// Explicit template instantiations
template bool ConfigManager::getConfig<bool>(const std::string, const bool&);
template std::string ConfigManager::getConfig<std::string>(const std::string, const std::string&);
template int ConfigManager::getConfig<int>(const std::string, const int&);
template double ConfigManager::getConfig<double>(const std::string, const double&);
template float ConfigManager::getConfig<float>(const std::string, const float&);

template void ConfigManager::setConfig<bool>(const std::string, const bool&);
template void ConfigManager::setConfig<std::string>(const std::string, const std::string&);
template void ConfigManager::setConfig<const char*>(const std::string, const char* const&);
template void ConfigManager::setConfig<int>(const std::string, const int&);
template void ConfigManager::setConfig<double>(const std::string, const double&);
template void ConfigManager::setConfig<float>(const std::string, const float&);