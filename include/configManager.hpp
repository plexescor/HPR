#pragma once

#include <string>
#include <vector>
#include <utility>

class ConfigManager
{
    public: //No need for destructor
        ConfigManager();

        template <typename T>
        T getConfig(const std::string requestedParam, const T& defaultValue);

        template <typename T>
        void setConfig(const std::string paramName, const T& value);

    private:
        void loadConfig();
        void saveConfig();
    
    private:
        std::vector<std::pair<std::string, std::string>> config; 
        std::string fileName = "config.csv";
        std::string filePath;
};