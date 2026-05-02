#pragma once
#include <string>

void getCurrentWindow_Init();
void getCurrentWindow_Loop();
//A singular function to return currently active window. Does platform specific calling and validating automatically
std::string getCurrentWindow();
std::string getCurrentWindow_Hyprland();
std::string getCurrentWindow_Windows();