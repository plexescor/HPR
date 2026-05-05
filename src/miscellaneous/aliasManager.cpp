#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

#include "aliasManager.hpp"

AliasManager::AliasManager() { loadAliases(); }

void AliasManager::loadAliases()
{
    // Yo filesystdm is sick
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
    aliasList.clear(); // clear before reload

    std::ifstream file(csvPath);
    if (!file.is_open())
    {
        std::cerr << "Warning: aliases.csv not found at " << csvPath
                  << ". Using raw names.\n";
        return;
    }

    // store last modified time
    lastModified = std::filesystem::last_write_time(csvPath);

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
    if (std::filesystem::exists(csvPath))
    {
        auto currentModified = std::filesystem::last_write_time(csvPath);
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