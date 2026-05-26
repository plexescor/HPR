#pragma once

#include <unordered_map>
#include <string>
#include <filesystem>
#include <vector>

class AliasManager
{
    public: //No need for destructor
        AliasManager();
        std::string getAlias(const std::string& rawName);
        std::string getAlias_Tab(const std::string& rawName);
        std::string getAlias_Project(const std::string& rawName);

    private:
        void loadAliases();
        void loadAliases_Tab();
        void loadAliases_Project();
    
    private:
        std::string fileName = "aliases.csv";
        std::string fileName_Tab = "tabAliases.csv";
        std::string fileName_Project = "projectAliases.csv";

        std::string filePath;
        std::string filePath_Tab;
        std::string filePath_Project;

        std::filesystem::file_time_type lastModified;
        std::filesystem::file_time_type lastModified_Tab;
        std::filesystem::file_time_type lastModified_Project;

        std::vector<std::pair<std::string, std::string>> aliasList;
        std::unordered_map<std::string, std::string> cacheDictionary;

        std::vector<std::pair<std::string, std::string>> aliasList_Tab;
        std::unordered_map<std::string, std::string> cacheDictionary_Tab;

        std::vector<std::pair<std::string, std::string>> aliasList_Project;
        std::unordered_map<std::string, std::string> cacheDictionary_Project;
};