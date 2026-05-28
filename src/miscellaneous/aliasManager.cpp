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
    loadAliases_Project();
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

void AliasManager::loadAliases_Project()
{
    aliasList_Project.clear(); //clear the vector if we ever reload 
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
    filePath_Project = tempPath + fileName_Project;

    std::ifstream file(filePath_Project);

    if (!file.is_open())
    {
        std::cerr << "Warning: projectAliases.csv not found at " << filePath_Project
                  << ". will use raw but parsed names.\n";
        return;
    }

    // store last modified time
    lastModified_Project = std::filesystem::last_write_time(filePath_Project);

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
            aliasList_Project.push_back({line.substr(0, commaPos), line.substr(commaPos + 1)});
        }
    }
}

std::string AliasManager::getAlias(const std::string &rawName)
{
    std::lock_guard lock(mutex);
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
            reverseCacheDictionary.clear();
            loadAliases();
        }
    }
    
    std::string prettyName;
    auto it = cacheDictionary.find(lowerName);
    if (it != cacheDictionary.end())
    {
        prettyName = it->second;
    }
    else
    {
        bool found = false;
        for (const auto &[triggerWord, pName] : aliasList)
        {
            if (lowerName.contains(triggerWord))
            {
                cacheDictionary[lowerName] = pName;
                prettyName = pName;
                found = true;
                break;
            }
        }
        if (!found)
        {
            cacheDictionary[lowerName] = rawName;
            prettyName = rawName;
        }
    }

    std::string lowerPretty = prettyName;
    std::transform(lowerPretty.begin(), lowerPretty.end(), lowerPretty.begin(),
        [](unsigned char c) { return std::tolower(c); });
    reverseCacheDictionary[lowerPretty] = rawName;

    return prettyName;
}

std::string AliasManager::getAlias_Tab(const std::string &rawName)
{
    std::lock_guard lock(mutex);
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
            reverseCacheDictionary_Tab.clear();
            loadAliases_Tab();
        }
    }
    
    std::string prettyName;
    auto it = cacheDictionary_Tab.find(lowerName);
    if (it != cacheDictionary_Tab.end())
    {
        prettyName = it->second;
    }
    else
    {
        bool found = false;
        for (const auto &[triggerWord, pName] : aliasList_Tab)
        {
            if (lowerName.contains(triggerWord))
            {
                cacheDictionary_Tab[lowerName] = pName;
                prettyName = pName;
                found = true;
                break;
            }
        }
        if (!found)
        {
            cacheDictionary_Tab[lowerName] = rawName;
            prettyName = rawName;
        }
    }

    std::string lowerPretty = prettyName;
    std::transform(lowerPretty.begin(), lowerPretty.end(), lowerPretty.begin(),
        [](unsigned char c) { return std::tolower(c); });
    reverseCacheDictionary_Tab[lowerPretty] = rawName;

    return prettyName;
}

std::string AliasManager::getAlias_Project(const std::string &rawName)
{
    std::lock_guard lock(mutex);
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
    if (std::filesystem::exists(filePath_Project))
    {
        auto currentModified = std::filesystem::last_write_time(filePath_Project);
        if (currentModified > lastModified_Project)
        {
            cacheDictionary_Project.clear();
            reverseCacheDictionary_Project.clear();
            loadAliases_Project();
        }
    }
    
    std::string prettyName;
    auto it = cacheDictionary_Project.find(lowerName);
    if (it != cacheDictionary_Project.end())
    {
        prettyName = it->second;
    }
    else
    {
        bool found = false;
        for (const auto &[triggerWord, pName] : aliasList_Project)
        {
            if (lowerName.contains(triggerWord))
            {
                cacheDictionary_Project[lowerName] = pName;
                prettyName = pName;
                found = true;
                break;
            }
        }
        if (!found)
        {
            cacheDictionary_Project[lowerName] = rawName;
            prettyName = rawName;
        }
    }

    std::string lowerPretty = prettyName;
    std::transform(lowerPretty.begin(), lowerPretty.end(), lowerPretty.begin(),
        [](unsigned char c) { return std::tolower(c); });
    reverseCacheDictionary_Project[lowerPretty] = rawName;

    return prettyName;
}

std::string AliasManager::getReverseAlias(const std::string &aliasName)
{
    std::lock_guard lock(mutex);
    std::string lowerName = aliasName;

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
            reverseCacheDictionary.clear();
            loadAliases();
        }
    }
    
    // check cache first
    auto it = reverseCacheDictionary.find(lowerName);

    // return if found in O(1)
    if (it != reverseCacheDictionary.end())
    {
        return it->second;
    }

    for (const auto &[triggerWord, prettyName] : aliasList)
    {
        std::string lowerPretty = prettyName;

        std::transform(lowerPretty.begin(),
            lowerPretty.end(),
            lowerPretty.begin(),
            [](unsigned char c)
            {
                return std::tolower(c);
            });

        if (lowerName.contains(lowerPretty))
        {
            reverseCacheDictionary[lowerName] = triggerWord;
            return triggerWord;
        }
    }

    // if nowhere, then return it as it is and save it in hashmap
    reverseCacheDictionary[lowerName] = aliasName;
    return aliasName;
}

std::string AliasManager::getReverseAlias_Tab(const std::string &aliasName)
{
    std::lock_guard lock(mutex);
    std::string lowerName = aliasName;

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
            reverseCacheDictionary_Tab.clear();
            loadAliases_Tab();
        }
    }
    
    // check cache first
    auto it = reverseCacheDictionary_Tab.find(lowerName);

    // return if found in O(1)
    if (it != reverseCacheDictionary_Tab.end())
    {
        return it->second;
    }

    for (const auto &[triggerWord, prettyName] : aliasList_Tab)
    {
        std::string lowerPretty = prettyName;

        std::transform(lowerPretty.begin(),
            lowerPretty.end(),
            lowerPretty.begin(),
            [](unsigned char c)
            {
                return std::tolower(c);
            });

        if (lowerName.contains(lowerPretty))
        {
            reverseCacheDictionary_Tab[lowerName] = triggerWord;
            return triggerWord;
        }
    }

    // if nowhere, then return it as it is and save it in hashmap
    reverseCacheDictionary_Tab[lowerName] = aliasName;
    return aliasName;
}

std::string AliasManager::getReverseAlias_Project(const std::string &aliasName)
{
    std::lock_guard lock(mutex);
    std::string lowerName = aliasName;

    //Convert to lower case
    std::transform(lowerName.begin(), 
        lowerName.end(), 
        lowerName.begin(),
        [](unsigned char c)
        { 
            return std::tolower(c); 
        });


    // hot reload check
    if (std::filesystem::exists(filePath_Project))
    {
        auto currentModified = std::filesystem::last_write_time(filePath_Project);
        if (currentModified > lastModified_Project)
        {
            reverseCacheDictionary_Project.clear();
            loadAliases_Project();
        }
    }
    
    // check cache first
    auto it = reverseCacheDictionary_Project.find(lowerName);

    // return if found in O(1)
    if (it != reverseCacheDictionary_Project.end())
    {
        return it->second;
    }

    for (const auto &[triggerWord, prettyName] : aliasList_Project)
    {
        std::string lowerPretty = prettyName;

        std::transform(lowerPretty.begin(),
            lowerPretty.end(),
            lowerPretty.begin(),
            [](unsigned char c)
            {
                return std::tolower(c);
            });

        if (lowerName.contains(lowerPretty))
        {
            reverseCacheDictionary_Project[lowerName] = triggerWord;
            return triggerWord;
        }
    }

    // if nowhere, then return it as it is and save it in hashmap
    reverseCacheDictionary_Project[lowerName] = aliasName;
    return aliasName;
}