#pragma once
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <filesystem>
#include "sol.hpp"
#include "databaseManager.hpp"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

struct LoadedExtension 
{
    std::filesystem::path path;
    sol::state lua;
    std::thread thread;
    std::atomic<bool> running{true};
    std::pair<std::string, std::string> identity;

    LoadedExtension() = default;
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
        ExtensionManager(DatabaseManager& dbm, bool dynamicLibraryExtensionLoading = false);
        ~ExtensionManager();
        void run();
        void loadExtensions();

    private:
        void updateExtensionPath();
        void loadNativeExtension(const std::filesystem::path& path);
        void registerFunctions(sol::state& lua);
        void runExtension(LoadedExtension& ext);
        void runNativeExtension(NativeExtension& ext);

    private:
        bool allowDynamicLibraryExtensionLoading;
        std::vector<std::unique_ptr<LoadedExtension>> extensions;
        std::vector<std::unique_ptr<NativeExtension>> nativeExtensions;
        std::string extensionPath;
        DatabaseManager& dbManager;
};