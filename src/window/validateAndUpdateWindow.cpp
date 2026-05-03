#include <string>
#include <algorithm>
#include <cctype>

#include "validateAndUpdateWindow.hpp"

//A cross platform beefy function to modify the currently active window name for readibility and fucking trash out
std::string validateAndUpdateWindow_Cross(std::string &windowName)
{
    stripTrailing(windowName);
    std::string unModifiedWindowName = windowName;

    //Convert windowName to lowercase
    std::transform(windowName.begin(), 
        windowName.end(), 
        windowName.begin(),
        [](unsigned char c)
        { 
            return std::tolower(c); 
        });

    //Windows OS checks for windows's specific garbage stuff, dont need guards
    if (windowName == ""
        || windowName.contains("explorer") 
        || windowName.contains("searchhost")
        || windowName.contains("openwith")
        || windowName.contains("startmenuexperiencehost")
        || windowName.contains("applicationframehost")
        || windowName.contains("shellexperiencehost"))
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

    return unModifiedWindowName; //Return unmodified name with proper Case
}

void stripTrailing(std::string &str)
{
    while (!str.empty() && (str.back() == '\n' || str.back() == '\r'))
        str.pop_back();
}
