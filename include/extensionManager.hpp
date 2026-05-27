#pragma once
#include <string>

class ExtensionManager 
{
    public:
        ExtensionManager();
        ~ExtensionManager();

        void loadExtensions();

    private:
        void updateExtensionPath();

    private:
        sol::state lua;
        std::string extensionPath;
};