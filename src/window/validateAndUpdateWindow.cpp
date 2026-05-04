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
        || windowName.contains("searchhost")
        || windowName.contains("openwith")
        || windowName.contains("startmenuexperiencehost")
        || windowName.contains("applicationframehost")
        || windowName.contains("shellexperiencehost")
        || windowName.contains("plasmashell")
        || windowName.contains("js::")
        || windowName.contains("null"))
    {
        return "Unknown";
    }

    //Mixed OS updation for commonly used apps
    //I know its inefficient but idc
    //Btw its c++23 method, i am talking about contains();
    //As thif func runs on background thread, it wont cause any performance issues, and if it does, then
    //well i dont care
    if (windowName.contains("chrome")) return "Chrome";
    else if (windowName.contains("dolphin")) return "Dolphin";
    else if (windowName.contains("code")) return "Visual Studio Code"; 
    else if (windowName.contains("msedge")) return "Edge";
    else if (windowName.contains("devenv")) return "Visual Studio";
    else if (windowName.contains("btop")) return "BTOP++";
    else if (windowName.contains("obs")) return "OBS Studio";
    else if (windowName.contains("spotify")) return "Spotify";
    else if (windowName.contains("discord")) return "Discord";
    else if (windowName.contains("steam")) return "Steam";
    else if (windowName.contains("notepad")) return "Notepad";
    else if (windowName.contains("terminal")) return "Terminal";
    else if (windowName.contains("cmd")) return "Command Prompt";
    else if (windowName.contains("powershell")) return "PowerShell";
    else if (windowName.contains("firefox")) return "Firefox";
    else if (windowName.contains("slack")) return "Slack";
    else if (windowName.contains("teams")) return "Microsoft Teams";
    else if (windowName.contains("zoom")) return "Zoom";
    else if (windowName.contains("vlc")) return "VLC Media Player";
    else if (windowName.contains("obsidian")) return "Obsidian";
    else if (windowName.contains("postman")) return "Postman";
    else if (windowName.contains("ptyxis")) return "Gnome-Terminal";//linux apps also and windows apps also
    else if (windowName.contains("gnome-terminal")) return "Gnome-Terminal";
    else if (windowName.contains("x-terminal-emulator")) return "X Terminal Emulator";
    else if (windowName.contains("xfce4-terminal")) return "XFCE4 Terminal";
    else if (windowName.contains("konsole")) return "Konsole";
    else if (windowName.contains("tilix")) return "Tilix";
    else if (windowName.contains("alacritty")) return "Alacritty";
    else if (windowName.contains("kitty")) return "Kitty";
    else if (windowName.contains("hyper")) return "Hyper Terminal";
    else if (windowName.contains("terminator")) return "Terminator";
    else if (windowName.contains("guake")) return "Guake Terminal";
    else if (windowName.contains("tilda")) return "Tilda Terminal";
    else if (windowName.contains("notion")) return "Notion";
    else if (windowName.contains("nautilus")) return "Nautilus";

    return unModifiedWindowName; //Return unmodified name with proper Case
}

void stripTrailing(std::string &str)
{
    while (!str.empty() && (str.back() == '\n' || str.back() == '\r'))
        str.pop_back();
}
