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

std::mutex stateMutex;

std::string currentPlatform = "";
bool coldBoot = true;
int frame = 0;
std::string window = "";

//Responsible for current platform and calling the getCurrentWindow_Loop() on another thread;
void getCurrentWindow_Init()
{
    if (coldBoot)
    {
        coldBoot = false;

        #ifdef __linux__ 
            //Get current desktop environment, linux only
            std::string currentPlatformCommand = "echo $XDG_CURRENT_DESKTOP";
            currentPlatform = runSystemCommand(currentPlatformCommand);
        #endif

        #ifdef _WIN32
            currentPlatform = "Windows";
        #endif
    }
    std::cout << "Init!\n";
    std::thread(getCurrentWindow_Loop).detach();
}

//Responsible for calling getCurrentWindow() and setting the result to AppState (state) struct so shit can access that
void getCurrentWindow_Loop()
{
    while (true)
    {
        window = getCurrentWindow();

        {
            std::lock_guard<std::mutex> lock(stateMutex);
            state.currentWindow = window;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

//A singular function to return currently active window. Does platform specific calling and validating automatically
std::string getCurrentWindow()
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

std::string getCurrentWindow_Hyprland()
{
    std::string command = "hyprctl activewindow -j | jq -r '.class'";
    
    // std::cout << "getcurrhypr: " << runSystemCommand(command) << std::endl;
    return runSystemCommand(command);
    
}

std::string getCurrentWindow_Windows()
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