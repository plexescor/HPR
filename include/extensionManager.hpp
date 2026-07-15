#pragma once
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <filesystem>
#include "sol.hpp"
#include "databaseManager.hpp"
#include "trayManager.hpp"
#include "linuxUtilities.hpp"
#include "currentWindowManager.hpp"
#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

#include <mutex>
#include <optional>
#include <map>

#include "appEvents.hpp"

//FORWARD DECLARE TO AVOID CIRCULAR DEPENDENCIES
class HPR;
class HPRInterpreter;

struct LoadedExtension 
{
    std::filesystem::path path;
    sol::state lua;
    std::thread thread;
    std::atomic<bool> running{true};
    std::atomic<bool> detached{false};
    std::pair<std::string, std::string> identity;
    std::recursive_mutex luaMutex; // Guards all shared Lua VM operations recursively to prevent multi-threaded race conditions without deadlocking
    std::recursive_mutex serverMutex;
    std::vector<std::pair<EventKey, size_t>> registeredConnections; // Tracks registered EventHub connections to clean up on destruction

    std::map<std::string, std::map<size_t, sol::function>> eventCallbacks;
    std::mutex eventQueueMutex;
    std::vector<std::pair<std::string, CppValue>> pendingEvents;

    LoadedExtension() = default;
    ~LoadedExtension();
    LoadedExtension(const LoadedExtension&) = delete;
    LoadedExtension& operator=(const LoadedExtension&) = delete;
};

class ExtensionManager 
{
    public:
        ExtensionManager();
        ~ExtensionManager();
        void run();
        void loadExtensions();

        void reloadExtension(std::string authorName, std::string extensionName);
        void unloadExtension(std::string authorName, std::string extensionName);
        void reloadAllExtensions();
        void refresh();

        std::optional<CppValue> dispatchOverride(const std::string& overrideName, const std::vector<CppValue>& args);

    private:
        void updateExtensionPath();
        void registerFunctions(LoadedExtension& ext);
        void runExtension(std::shared_ptr<LoadedExtension> ext);

    private:
        std::vector<std::shared_ptr<LoadedExtension>> extensions;
        std::filesystem::path extensionPath;

        bool didTimeoutDuringUnload = false;

    public:
        //some shit
        DatabaseManager* dbManager = nullptr;
        TrayManager* trayManager = nullptr;
        CurrentWindowManager* currentWindowManager = nullptr;
        HPR* app = nullptr;
        HPRInterpreter* interpreterApp = nullptr;
        LinuxInitialiser* linuxInit = nullptr;
};

std::filesystem::path resolveAndSecurePath(const std::string& userPath, const std::filesystem::path& baseDir, std::string& errOut);