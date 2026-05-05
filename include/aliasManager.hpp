#pragma once

#include <unordered_map>
#include <string>
#include <vector>

class AliasManager
{
    public: //No need for destructor
        AliasManager();
        std::string getAlias(const std::string& rawName);

    private:
        void loadAliases();
    
    private:
        std::string fileName = "aliases.csv";
        std::vector<std::pair<std::string, std::string>> aliasList;
        std::unordered_map<std::string, std::string> cacheDictionary;
};