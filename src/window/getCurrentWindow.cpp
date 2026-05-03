#include <string>
#include <iostream>

#include "getCurrentWindow.hpp"
#include "validateAndUpdateWindow.hpp"
#include "windowUtilities.hpp"
#include "appState.hpp"

#ifdef _WIN32 //Include windows headers
#include <windows.h>
#include <psapi.h>
#endif


#include <mutex>
#include <thread>
#include <chrono>

CurrentWindowManager::CurrentWindowManager()
{
    #ifdef __linux__ 
        //Get current desktop environment, linux only
        std::string currentPlatformCommand = "echo $XDG_CURRENT_DESKTOP";
        currentPlatform = runSystemCommand(currentPlatformCommand);
    #endif

    #ifdef _WIN32
        currentPlatform = "Windows";
    #endif
}

CurrentWindowManager::~CurrentWindowManager()
{
    running = false;

    if(windowPollingThread.joinable()) windowPollingThread.join();
}

void CurrentWindowManager::run()
{
    std::cout << "Running Loop!\n";

    windowPollingThread = std::thread(&CurrentWindowManager::getCurrentWindow_Loop, this);
}

//Responsible for calling getCurrentWindow() and setting the result to AppState (state) struct so shit can access that
void CurrentWindowManager::getCurrentWindow_Loop()
{
    auto lastTimestamp = std::chrono::steady_clock::now(); //Get the tick's time
    while (running)
    {
        window = getCurrentWindow();

        {
            //Update current window in the AppState
            std::lock_guard<std::mutex> lock(AppState::stateMutex);
            AppState::state.currentWindow = window;

            auto now = std::chrono::steady_clock::now(); //get time now
            auto elapsed = now - lastTimestamp;
            lastTimestamp = now;
            AppState::state.timeLog_PerApp[window] += std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

//A singular function to return currently active window. Does platform specific calling and validating automatically
std::string CurrentWindowManager::getCurrentWindow()
{
    //Run the *expensive* command every 10fps //100ms on 60hz
    // if (frame != 10)
    // {
    //     frame++;
    //     return window;
    // }
    
    // frame = 0;

    if (currentPlatform.contains("Hyprland"))
    {
        window = getCurrentWindow_Hyprland();
    }

    else if (currentPlatform.contains("Windows"))
    {
        window = getCurrentWindow_Windows();
    }

    //Need to explicitly set this because shit happens if cached value is returned 
    window = validateAndUpdateWindow_Cross(window);
    return window;
}

std::string CurrentWindowManager::getCurrentWindow_Hyprland()
{
    std::string command = "hyprctl activewindow -j | jq -r '.class'";
    
    // std::cout << "getcurrhypr: " << runSystemCommand(command) << std::endl;
    return runSystemCommand(command);
    
}

std::string CurrentWindowManager::getCurrentWindow_Windows()
{
    #ifdef _WIN32
    //Get active (foreground) window
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return "";

    char title[512];
    int length = GetWindowTextA(hwnd, title, sizeof(title));

    if (length == 0) return "";
    return std::string(title, length);
    
    #endif
    return ""; //to shut up compiler on linux
}