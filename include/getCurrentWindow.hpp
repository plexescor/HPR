#pragma once
#include <string>

//A singular function to return currently active window. Does platform specific calling and validating automatically
std::string getCurrentWindow();

std::string getCurrentWindow_Hyprland();