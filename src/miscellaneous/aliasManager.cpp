#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

#include "aliasManager.hpp"

AliasManager::AliasManager() 
{ 
    loadAliases(); 
    loadAliases_Tab();
}

void AliasManager::loadAliases()
{
    aliasList.clear(); //clear the vector if we ever reload alises.csv
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
        std::cerr << "Warning: aliases.csv not found at " << filePath
                  << ". Using raw names.\n";
        return;
    }

    // store last modified time
    lastModified = std::filesystem::last_write_time(filePath);

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
            aliasList.push_back({line.substr(0, commaPos), line.substr(commaPos + 1)});
        }
    }
}

void AliasManager::loadAliases_Tab()
{
    aliasList_Tab.clear(); //clear the vector if we ever reload 
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
    filePath_Tab = tempPath + fileName_Tab;

    std::ifstream file(filePath_Tab);

    if (!file.is_open())
    {
        std::cerr << "Warning: tabAliases.csv not found at " << filePath_Tab
                  << ". Using raw names.\n";
        return;
    }

    // store last modified time
    lastModified_Tab = std::filesystem::last_write_time(filePath_Tab);

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
            aliasList_Tab.push_back({line.substr(0, commaPos), line.substr(commaPos + 1)});
        }
    }
}

std::string AliasManager::getAlias(const std::string &rawName)
{
    std::string lowerName = rawName;

    //Convert to lower case
    std::transform(lowerName.begin(), 
        lowerName.end(), 
        lowerName.begin(),
        [](unsigned char c)
        { 
            return std::tolower(c); 
        });


    // hot reload check
    if (std::filesystem::exists(filePath))
    {
        auto currentModified = std::filesystem::last_write_time(filePath);
        if (currentModified > lastModified)
        {
            cacheDictionary.clear();
            loadAliases();
        }
    }
    
    // check cache first
    auto it = cacheDictionary.find(lowerName);

    // return if found in O(1)
    if (it != cacheDictionary.end())
    {
        return it->second;
    }

    // else loop through the entire vector
    for (const auto &[triggerWord, prettyName] : aliasList)
    {
        if (lowerName.contains(triggerWord))
        {
            // found, save it to map and return
            cacheDictionary[lowerName] = prettyName;
            return prettyName;
        }
    }

    // if nowhere, then return it as it is and save it in hashmap
    cacheDictionary[lowerName] = rawName;
    return rawName;
}

std::string AliasManager::getAlias_Tab(const std::string &rawName)
{
    std::string lowerName = rawName;

    //Convert to lower case
    std::transform(lowerName.begin(), 
        lowerName.end(), 
        lowerName.begin(),
        [](unsigned char c)
        { 
            return std::tolower(c); 
        });


    // hot reload check
    if (std::filesystem::exists(filePath_Tab))
    {
        auto currentModified = std::filesystem::last_write_time(filePath_Tab);
        if (currentModified > lastModified_Tab)
        {
            cacheDictionary_Tab.clear();
            loadAliases_Tab();
        }
    }
    
    // check cache first
    auto it = cacheDictionary_Tab.find(lowerName);

    // return if found in O(1)
    if (it != cacheDictionary_Tab.end())
    {
        return it->second;
    }

    // else loop through the entire vector
    for (const auto &[triggerWord, prettyName] : aliasList_Tab)
    {
        if (lowerName.contains(triggerWord))
        {
            // found, save it to map and return
            cacheDictionary_Tab[lowerName] = prettyName;
            return prettyName;
        }
    }

    // if nowhere, then return it as it is and save it in hashmap
    cacheDictionary_Tab[lowerName] = rawName;
    return rawName;
}