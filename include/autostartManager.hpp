// autostartManager.hpp
#pragma once

class AutostartManager 
{
public:
    // Checks if HPR autostart is currently enabled in the OS.
    static bool isEnabled();

    // Sets/updates the autostart configuration in the OS.
    static void setEnabled(bool enable, bool headless);
};
