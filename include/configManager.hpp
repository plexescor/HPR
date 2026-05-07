#pragma once

#include <unordered_map>
#include <string>
#include <filesystem>
#include <vector>

class ConfigManager
{
    public: //No need for destructor
        ConfigManager();
        bool getConfig(const std::string& requestedParam, bool defaultValue);

    private:
        void loadConfig();
    
    private:
        std::vector<std::pair<std::string, std::string>> config; 
        std::string fileName = "config.csv";
        std::filesystem::path csvPath;
};