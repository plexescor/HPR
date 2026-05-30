#pragma once
#include "aliasManager.hpp"
#include "patternAnalyzer.hpp"
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <map>
#include <cstdint>
#include <vector>

namespace AppState {
    
    enum CurrentView {HISTORICAL_SINGULAR, LIVE};

    struct AppState {
        std::vector<std::pair<std::string, std::string>> loadedExtensions;

        CurrentView currentView = CurrentView::LIVE;

        bool useTabView = false; //which means data is shown per site and not per tab
        bool isRawProjectView = false; //which means VSCode projects are shown as they are, without aliasing or parsing
        std::string currentPlatform;

        std::string currentError = "";
        
        std::string currentWindow;
        std::string previousWindow;
        std::string currentTitle;
        
        //Apps
        std::map<std::string, long> timeLog_PerApp; 
        std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;

        //Tabs
        std::map<std::string, long> timeLog_PerTab;

        //Projects
        std::map<std::string, long> timeLog_PerProject;
    };

    //Intended to hold data for max 1 whole day
    struct HistoricalData {
        std::map<std::string, long> timeLog_PerApp;
        std::map<std::string, long> timeLog_PerTab;
        std::map<std::string, long> timeLog_PerProject;
        std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;
        bool isLoaded = false;
    };

    extern AppState state;
    extern HistoricalData historicalData_State;

    extern AliasManager aliasManager;
    extern PatternAnalyzer patternAnalyzer;

    extern std::mutex patternAnalyzerMutex;

    extern std::mutex stateMutex;
    extern std::mutex historyStateMutex;
    extern std::mutex historyLoadedMutex;
    extern std::condition_variable historyLoadedCV;
}
