#pragma once
#include <string>
#include <atomic>
#include <map>

namespace AppState {
    
    struct AppState {
        std::string currentWindow;
        std::string previousWindow;
        std::map<std::string, long> timeLog_PerApp; 
    };
    extern AppState state;
    extern std::mutex stateMutex;
}
