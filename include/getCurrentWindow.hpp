#pragma once
#include <string>
#include <thread>
#include <atomic>


class CurrentWindowManager
{
    public:
        CurrentWindowManager();
        ~CurrentWindowManager();
        void run();
    
    private:
        void getCurrentWindow_Loop();

        //A singular function to return currently active window. Does platform specific calling and validating automatically
        std::string getCurrentWindow();
        std::string getCurrentWindow_Hyprland();
        std::string getCurrentWindow_Windows();
        std::string getCurrentWindow_Gnome();
        std::string getCurrentWindow_KDE();

        std::string getCurrentTab();
        std::string getCurrentTab_Hyprland();
        std::string getCurrentTab_Windows();
        std::string getCurrentTab_Gnome();
        std::string getCurrentTab_KDE();

    private:
        std::string currentPlatform = "";
        std::string qdbusCmd = "qdbus6";
        std::string window = "";
        std::string previousWindow = "";

        std::string tab = "";

        std::thread windowPollingThread;
        std::atomic<bool> running{true};
};
