#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>

#include "sol.hpp"

#include "extensionManager.hpp"


ExtensionManager::ExtensionManager() 
{
    loadExtensions();
}

void ExtensionManager::loadExtensions()
{
    updateExtensionPath();

    //load all lua files from this dir recursively
    for (const auto& entry : std::filesystem::recursive_directory_iterator(extensionPath))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".lua")
        {
            lua.script_file(entry.path().string());
        }
    }
}

void ExtensionManager::updateExtensionPath()
{
    #ifdef _WIN32
        extensionPath = std::getenv("APPDATA");
        extensionPath += "/HPR/HPR_Config/Extensions/";
    #else
        const char* home = std::getenv("HOME");
        if (!home) throw std::runtime_error("HOME env var not set");
        extensionPath = home;
        extensionPath += "/.config/HPR/Extensions/";
    #endif

    std::filesystem::create_directories(extensionPath);
}