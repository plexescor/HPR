#pragma once
#include <string>
#include <thread>
#include <atomic>

#include "windowBackendRegistery.hpp"

class CurrentWindowManager
{
public:
    CurrentWindowManager();
    ~CurrentWindowManager();

    void run();

    static void stopTracking();
    static void startTracking();

private:
    void getCurrentWindow_Loop();
    void detectAndSetBackend();

    std::string getCurrentWindow();
    std::string getCurrentTitle();

private:
    WindowBackend* activeBackend = nullptr;

    std::string currentPlatform = "";
    std::string qdbusCmd = "qdbus6";
    std::string window = "";
    std::string previousWindow = "";

    std::string tab = "";
    std::string project = "";

    std::thread windowPollingThread;
    std::atomic<bool> running{true};

    static CurrentWindowManager* instance;
};