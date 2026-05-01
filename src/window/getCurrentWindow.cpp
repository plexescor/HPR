#include <string>
#include <iostream>

#include "getCurrentWindow.hpp"
#include "validateAndUpdateWindow.hpp"
#include "windowUtilities.hpp"

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
    }

    //Run the *expensive* command every 30fps //500ms on 60hz
    if (frame != 30)
    {
        frame++;
        return window;
    }
    
    frame = 0;

    if (currentPlatform.contains("Hyprland"))
    {
        window = getCurrentWindow_Hyprland();
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