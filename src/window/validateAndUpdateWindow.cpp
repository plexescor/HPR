#include <string>

#include "validateAndUpdateWindow.hpp"

//A cross platform beefy function to modify the currently active window name for readibility and fucking trash out
std::string validateAndUpdateWindow_Cross(std::string &windowName)
{
    //Windows OS checks for windows's specific garbage stuff, dont need guards
    if (windowName == ""
        || windowName.contains("Explorer") 
        || windowName.contains("SearchHost")
        || windowName.contains("OpenWith")
        || windowName.contains("StartMenuExperienceHost")
        || windowName.contains("ApplicationFrameHost")
        || windowName.contains("ShellExperienceHost"))
    {
        return "Unknown";
    }

    //Mixed OS updation for commonly used apps
    //I know its inefficient but idc
    //Btw its c++23 method, i am talking about contains();
    if (windowName.contains("chrome")) return "Chrome";
    else if (windowName.contains("code")) return "Visual Studio Code"; 
    else if (windowName.contains("msedge")) return "Edge";
    else if (windowName.contains("devenv")) return "Visual Studio";
    else if (windowName.contains("btop")) return "BTOP++";
    else if (windowName.contains("obs")) return "OBS Studio";

    return windowName; //when not sure 
}