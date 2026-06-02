#include <string>
#include <algorithm>
#include <cctype>

#include "validateAndUpdateWindow.hpp"
#include "appState.hpp"
#include "extensionManager.hpp"

//A cross platform beefy function to modify the currently active window name for readibility and fucking trash out
std::string validateAndUpdateWindow_Cross(std::string &windowName)
{
    stripTrailing(windowName);
    if (AppState::extManager)
    {
        auto res = AppState::extManager->dispatchOverride("validateAndUpdateWindow_Cross", { CppValue(CppValue::Type::String, windowName) });
        if (res.has_value() && res->type == CppValue::Type::String)
        {
            return res->str_val;
        }
    }
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
        || windowName.contains("searchhost")
        || windowName.contains("explorer")
        || windowName.contains("shellhost")
        || windowName.contains("openwith")
        || windowName.contains("startmenuexperiencehost")
        || windowName.contains("applicationframehost")
        || windowName.contains("shellexperiencehost")
        || windowName.contains("plasmashell")
        || windowName.contains("no backend")
        || windowName.contains("js::")
        || windowName.contains("null")
        || windowName.contains("lockapp")
        || windowName.contains("{}") //on cinammon when HPR is lauched via desktop file, the first window is always {}, this is a quick fix for that
        || windowName.contains("please wait"))
    {
        return "Unknown";
    }

    return unModifiedWindowName; //Return unmodified name with proper Case
}

void stripTrailing(std::string &str)
{
    while (!str.empty() && (str.back() == '\n' || str.back() == '\r'))
        str.pop_back();
}
