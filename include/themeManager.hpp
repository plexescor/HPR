#pragma once

#include <string>
#include <map>
#include <vector>
#include <filesystem>
class ThemeManager
{
    public:
        ThemeManager();

        void reload();
        void initialisePath();
        std::string getPathByName(std::string name);

    public:
        //Names version and  paths
        std::map<std::pair<std::string, std::string> , std::string> availableThemes;
        std::map<std::string, std::vector<std::string>> themePreview;
        std::map<std::string, std::string> availableThemes_Bare;

        std::filesystem::path themeDirectory;

        bool areThemesAvailable = false;
};
