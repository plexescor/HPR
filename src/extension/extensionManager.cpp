#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex> 
#include "sol.hpp"

#include "appState.hpp"
#include "extensionManager.hpp"


ExtensionManager::ExtensionManager() 
{
    loadExtensions();
}

ExtensionManager::~ExtensionManager()
{
    //Join all threads vro
    for (auto& ext : extensions)
    {
        ext->running = false;
        if (ext->thread.joinable())
            ext->thread.join();
    }
}

void ExtensionManager::loadExtensions()
{
    updateExtensionPath();

    //load all lua files from this dir recursively
    for (const auto& entry : std::filesystem::recursive_directory_iterator(extensionPath))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".lua")
        {
            auto ext = std::make_unique<LoadedExtension>();
            ext->path = entry.path();
            
            registerFunctions(ext->lua);
            ext->lua.script_file(entry.path().string());
            
            ext->thread = std::thread(&ExtensionManager::runExtension, this, std::ref(*ext));
            
            extensions.push_back(std::move(ext));
        }
    }
}

void ExtensionManager::runExtension(LoadedExtension& ext)
{
    sol::function onTick = ext.lua["onTick"];
    while (ext.running)
    {
        if (onTick.valid()) onTick();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void ExtensionManager::registerFunctions(sol::state& lua)
{
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table);
    
    lua["HPR"] = lua.create_table();
    
    lua["HPR"]["getActiveApp"] = []() 
    {
        std::lock_guard lock(AppState::stateMutex);
        return AppState::state.currentWindow;
    };
    
    lua["HPR"]["getTimeForApp"] = [](std::string app) 
    {
        std::lock_guard lock(AppState::stateMutex);
        return AppState::state.timeLog_PerApp[app];
    };
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