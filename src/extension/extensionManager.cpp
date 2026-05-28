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
#include "window_E.hpp"
#include "windowUtilities.hpp"
#include "windowBackendRegistery.hpp"


ExtensionManager::ExtensionManager(DatabaseManager& dbm) : dbManager(dbm)
{
    loadExtensions();
}

ExtensionManager::~ExtensionManager()
{
    //two passes for "fastness"
    for (auto& ext : extensions)
    {
        ext->running = false;
    }

    for (auto& ext : extensions)
    {
        if (ext->thread.joinable())
            ext->thread.join();
    }
    registeredBackends.clear();
}

void ExtensionManager::run()
{
    for (auto& ext : extensions)
    {
        ext->thread = std::thread(
            &ExtensionManager::runExtension,
            this,
            std::ref(*ext)
        );
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

            try
            {
                ext->lua.script_file(entry.path().string());
            }
            catch (const std::exception& e)
            {
                std::cerr << "Failed to load extension: "
                        << entry.path()
                        << "\nError: " << e.what() << '\n';
                continue;
            }
                        
            extensions.push_back(std::move(ext));
        }
        else if (entry.is_regular_file())
        {
            std::cerr << "Skipping non-lua file: " << entry.path() << '\n';
        }
    }
}

void ExtensionManager::runExtension(LoadedExtension& ext)
{
    int sleepTime = 1000; //ms
    sol::function init = ext.lua["init"];
    sol::function onTick = ext.lua["onTick"];

    if (init.valid()) sleepTime = init();
    
    auto lastTime = std::chrono::high_resolution_clock::now();
    while (ext.running)
    {
        auto currentTime = std::chrono::high_resolution_clock::now();

        float delta = std::chrono::duration<float, std::milli>(
                        currentTime - lastTime
                    ).count();
            

        lastTime = currentTime;

        try
        {
            if (onTick.valid()) onTick(delta);          
        }
        catch (const std::exception& e)
        {
            std::cerr << "Extension error in "
                      << ext.path
                      << ": "
                      << e.what()
                      << '\n';
        }
        int slept = 0;
        while (ext.running && slept < sleepTime)
        {
            int chunk = (sleepTime - slept < 100) ? (sleepTime - slept) : 100;
            std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
            slept += chunk;
        }
    }
}

void ExtensionManager::registerFunctions(sol::state& lua)
{
    //Functions exposed to lua
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);
    
    lua["HPR"] = lua.create_table();
    
    lua["HPR"]["getCurrentWindow_E"] = []() 
    {
        return getCurrentWindow_E();
    };

    lua["HPR"]["getCurrentTitle_E"] = []() 
    {
        return getCurrentTitle_E();
    };

    lua["HPR"]["runSystemCommand_E"] = [](std::string command) 
    {
        return runSystemCommand(command);
    };

    lua["HPR"]["getAlias"] = [](std::string command) 
    {
        return AppState::aliasManager.getAlias(command);
    };

    lua["HPR"]["getAlias_Tab"] = [](std::string command) 
    {
        return AppState::aliasManager.getAlias_Tab(command);
    };

    lua["HPR"]["getAlias_Project"] = [](std::string command) 
    {
        return AppState::aliasManager.getAlias_Project(command);
    };

    lua["HPR"]["getReverseAlias"] = [](std::string aliasName) 
    {
        return AppState::aliasManager.getReverseAlias(aliasName);
    };

    lua["HPR"]["getReverseAlias_Tab"] = [](std::string aliasName) 
    {
        return AppState::aliasManager.getReverseAlias_Tab(aliasName);
    };

    lua["HPR"]["getReverseAlias_Project"] = [](std::string aliasName) 
    {
        return AppState::aliasManager.getReverseAlias_Project(aliasName);
    };

    lua["HPR"]["registerBackend_E"] = [](
        std::string name, 
        sol::function matchesEnvironment,
        sol::function initialize,
        sol::function isUsable,
        sol::function getCurrentWindow,
        sol::function getCurrentTitle
    ) 
    {
        registerBackend_E(name, matchesEnvironment, initialize, isUsable, getCurrentWindow, getCurrentTitle);
    };

    lua["HPR"]["dbExecute_E"] = [this](std::string sql, sol::optional<std::vector<std::string>> params) 
    {
        dbManager.executeSQL(sql, params.value_or(std::vector<std::string>{}));
    };

    lua["HPR"]["dbQuery_E"] = [this](std::string sql, sol::optional<std::vector<std::string>> params) 
    {
        return dbManager.querySQL(sql, params.value_or(std::vector<std::string>{}));
    };
}

void ExtensionManager::updateExtensionPath()
{
    #ifdef _WIN32
        extensionPath = std::getenv("APPDATA");
        extensionPath += "/HPR/HPR_Config/extensions/";
    #else
        const char* home = std::getenv("HOME");
        if (!home) throw std::runtime_error("HOME env var not set");
        extensionPath = home;
        extensionPath += "/.config/HPR/extensions/";
    #endif

    std::filesystem::create_directories(extensionPath);
}