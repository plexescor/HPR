#pragma once
#include <string>
#include <map>

#include "windowDetection.hpp"
#include "validateWindow.hpp"

namespace activeWindowAndDataManagement
{
    int updateCurrentWindowInfoAndData();
    int getCurrentSwitchCount();
    std::string getCurrentWindowName();
    std::map<std::string, double>* getTimeLog();
}
