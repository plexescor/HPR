#pragma once

#include <string>
#include <vector>
#include <utility>
#include <filesystem>
class ConfigManager
{
    public: //No need for destructor
        ConfigManager();

        template <typename T>
        T getConfig(const std::string requestedParam, const T& defaultValue);

        template <typename T>
        void setConfig(const std::string paramName, const T& value);

    public:
        bool isFirstLaunch() const { return firstLaunch; }
        bool isTelemetryPromptAnswered() const { return telemetryPromptAnswered; }
        void markTelemetryPromptAnswered() { telemetryPromptAnswered = true; }

    private:
        void loadConfig();
        void saveConfig();
    
    private:
        std::vector<std::pair<std::string, std::string>> config; 
        std::string fileName = "config.csv";
        std::filesystem::path filePath;
        bool firstLaunch = false;
        bool telemetryPromptAnswered = false;
};