#pragma once
#include <string>
#include <atomic>

struct AppState {
    std::string currentWindow;
    std::string previousWindow;
};

extern AppState state;
extern std::mutex stateMutex;