#include <string>
#include <iostream>

#include "getCurrentWindow.hpp"
#include "validateAndUpdateWindow.hpp"
#include "windowUtilities.hpp"

#ifdef _WIN32 //Include windows headers
#include <windows.h>
#include <psapi.h>
#endif

std::string currentPlatform = "";
bool coldBoot = true;
int frame = 0;
std::string window = "";

//A singular function to return currently active window. Does platform specific calling and validating automatically
std::string getCurrentWindow()
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

    //Run the *expensive* command every 30fps //500ms on 60hz
    // if (frame != 30)
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
}