#pragma once

#include <string>

// Returns process name of the currently focused window
// Example: "chrome.exe", "code.exe", "explorer.exe"
std::string getActiveProcessName();
