#include <unordered_map>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

#include "aliasManager.hpp"

AliasManager::AliasManager()
{
    loadAliases();
}

void AliasManager::loadAliases()
{
    std::ifstream file(fileName);

    // Check if file even exists
    if (!file.is_open()) 
    {
        std::cerr << "Warning: aliases.csv not found. Using raw names.\n";
        return;
    }

    std::string line;

    //Important for not crashing in 69gb files
    // get one line at a time
    while (std::getline(file, line)) 
    {
        //Fine comma, leftValue = rawName, right = newName 
        size_t commaPos = line.find(',');    

        //If skip or starts with #, continue
        //# is treated as comments
        if (line.empty() || line.starts_with("#")) 
        {
            continue;
        }

        //No Comma = skip, essentialy comments but not recommened to wrie comments this way
        if (commaPos != std::string::npos) 
        {
            // Left side, raw name
            std::string rawName = line.substr(0, commaPos);
            
            // Right side new name
            std::string prettyName = line.substr(commaPos + 1);

            aliasList.push_back({rawName, prettyName});
        }
    }
}

std::string AliasManager::getAlias(const std::string& rawName)
{
    // check cache first
    auto it = cacheDictionary.find(rawName);

    //return if found in O(1)
    if (it != cacheDictionary.end()) 
    {
        return it->second; 
    }

    // else loop through the entire vector
    for (const auto& [triggerWord, prettyName] : aliasList) 
    {
        if (rawName.contains(triggerWord)) 
        {
            // found, save it to map and return 
            cacheDictionary[rawName] = prettyName;
            return prettyName;
        }
    }
    
    // if nowhere, then return it as it is and save it in hashmap
    cacheDictionary[rawName] = rawName;
    return rawName;
}