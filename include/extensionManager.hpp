#pragma once
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "sol.hpp"
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
        ExtensionManager();
        ~ExtensionManager();
        void run();
        void loadExtensions();

    private:
        void updateExtensionPath();
        void registerFunctions(sol::state& lua);
        void runExtension(LoadedExtension& ext);

    private:
        std::vector<std::unique_ptr<LoadedExtension>> extensions;
        std::string extensionPath;
};