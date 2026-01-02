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

    //If no modificatino occur, return OG windowName;
    return windowName;

    #endif
}