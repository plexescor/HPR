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
#include "getCurrentWindow.hpp"
#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

#include <mutex>

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
    std::pair<std::string, std::string> identity;
    std::recursive_mutex luaMutex; // Guards all shared Lua VM operations recursively to prevent multi-threaded race conditions without deadlocking
    std::recursive_mutex serverMutex;
    std::vector<std::pair<EventKey, size_t>> registeredConnections; // Tracks registered EventHub connections to clean up on destruction

    LoadedExtension() = default;
    ~LoadedExtension();
    LoadedExtension(const LoadedExtension&) = delete;
    LoadedExtension& operator=(const LoadedExtension&) = delete;
};

struct NativeExtension 
{
    std::filesystem::path path;

#ifdef _WIN32
    HMODULE handle{nullptr};
#else
    void* handle{nullptr};
#endif

    std::thread thread;
    std::atomic<bool> running{true};

    NativeExtension() = default;
    NativeExtension(const NativeExtension&) = delete;
    NativeExtension& operator=(const NativeExtension&) = delete;
};

class ExtensionManager 
{
    public:
        ExtensionManager(bool dynamicLibraryExtensionLoading = false);
        ~ExtensionManager();
        void run();
        void loadExtensions();

        void reloadExtension(std::string authorName, std::string extensionName);
        void unloadExtension(std::string authorName, std::string extensionName);
        void reloadAllExtensions();
        void refresh();

    private:
        void updateExtensionPath();
        void loadNativeExtension(const std::filesystem::path& path);
        void registerFunctions(LoadedExtension& ext);
        void runExtension(LoadedExtension& ext);
        void runNativeExtension(NativeExtension& ext);

    private:
        bool allowDynamicLibraryExtensionLoading;
        std::vector<std::unique_ptr<LoadedExtension>> extensions;
        std::vector<std::unique_ptr<NativeExtension>> nativeExtensions;
        std::string extensionPath;

    public:
        //some shit
        DatabaseManager* dbManager;
        TrayManager* trayManager;
        CurrentWindowManager* currentWindowManager;
        HPR* app;
        HPRInterpreter* interpreterApp;
        LinuxInitialiser* linuxInit;
};

std::filesystem::path resolveAndSecurePath(const std::string& userPath, const std::filesystem::path& baseDir, std::string& errOut);