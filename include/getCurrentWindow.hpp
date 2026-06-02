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

    void stopTracking();
    void startTracking();
	void detectAndSetBackend();

private:
    void getCurrentWindow_Loop();

    std::string getCurrentWindow();
    std::string getCurrentTitle();
    std::string getCurrentPid();

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
};