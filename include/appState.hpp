#pragma once
#include <string>
#include <atomic>
#include <mutex>
#include <map>
#include <cstdint>
#include <vector>

namespace AppState {
    
    struct AppState {
        std::string currentPlatform;

        std::string currentWindow;
        std::string previousWindow;
        
        std::map<std::string, long> timeLog_PerApp; 

        std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;
    };
    extern AppState state;
    extern std::mutex stateMutex;
}
