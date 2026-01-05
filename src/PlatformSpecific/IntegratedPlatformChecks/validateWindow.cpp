#include <string>

bool isValidWindow(std::string windowName)
{

    //CHECKS FOR WINDOWS
    #ifdef _WIN32

    if (windowName != "" 
        && windowName != "Explorer" 
        && windowName != "SearchHost" 
        && windowName != "explorer" 
        && windowName != "OpenWith"
        && windowName != "StartMenuExperienceHost" 
        && windowName != "ApplicationFrameHost"
        && windowName != "ShellExperienceHost")
    {
        return true;
    }
    #endif

    #ifdef __linux__

    if (windowName != "") return true;
    return false;

    #endif

    return false;
}

std::string updateWindowName(std::string windowName)
{
    #ifdef _WIN32
    //I know its inefficient but idc
    //Btw its c++23 method, i am talking about contains();
    if (windowName.contains("chrome")) return "Chrome";
    else if (windowName.contains("msedge")) return "Edge";
    else if (windowName.contains("devenv")) return "Visual Studio";
    else if (windowName.contains("obs64")) return "OBS Studio";

    //If no modificatino occur, return OG windowName;
    return windowName;

    #endif

    #ifdef __linux__

    //return for now
    return windowName;

    #endif
}