#include <string>

bool isValidWindow(std::string windowName)
{

    //CHECKS FOR WINDOWS
    #ifdef _WIN32

    if (windowName != "" 
        && windowName != "Explorer" 
        && windowName != "SearchHost" 
        && windowName != "explorer" 
        && windowName != "StartMenuExperienceHost" 
        && windowName != "ApplicationFrameHost"
        && windowName != "ShellExperienceHost")
    {
        return true;
    }
    #endif

    return false;
}