#pragma once
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "sol.hpp"
#include "databaseManager.hpp"

#ifdef _WIN32
    #include <windows.h>
    std::vector<HMODULE> nativeHandles;
#else
    #include <dlfcn.h>
    std::vector<void*> nativeHandles;
#endif

struct LoadedExtension 
{
    std::filesystem::path path;
    sol::state lua;
    std::thread thread;
    std::atomic<bool> running{true};

    LoadedExtension() = default;
    LoadedExtension(const LoadedExtension&) = delete;
    LoadedExtension& operator=(const LoadedExtension&) = delete;
};

class ExtensionManager 
{
    public:
        ExtensionManager(DatabaseManager& dbm, bool dynamicLibraryExtensionLoading = false);
        ~ExtensionManager();
        void run();
        void loadExtensions();

    private:
        void updateExtensionPath();
        void loadNativeExtension(const std::filesystem::path& path);
        void registerFunctions(sol::state& lua);
        void runExtension(LoadedExtension& ext);

    private:
        bool allowDynamicLibraryExtensionLoading;
        std::vector<std::unique_ptr<LoadedExtension>> extensions;
        std::string extensionPath;
        DatabaseManager& dbManager;
};