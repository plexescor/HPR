#pragma once
#include "aliasManager.hpp"
#include "patternAnalyzer.hpp"
#include "configManager.hpp"
#include "themeManager.hpp"
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <map>
#include <cstdint>
#include <vector>

class ExtensionManager;

namespace AppState {
    
    enum CurrentView {HISTORICAL_SINGULAR, LIVE, HISTORICAL_NUMBER, HISTORICAL_RANGE};

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
        
        std::string nowPlayingTitle;
        std::string nowPlayingUrl;
        
        //Apps
        std::map<std::string, uint64_t> timeLog_PerApp; 
        std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;

        //idk
        // std::vector<std::pair<std::string, uint64_t>> switchEventLog;

        //Tabs
        std::map<std::string, uint64_t> timeLog_PerTab;

        //Projects
        std::map<std::string, uint64_t> timeLog_PerProject;

        // Limits & Goals
        std::map<std::string, int> appLimits;
        std::map<std::string, int> appGoals;
        std::map<std::string, uint64_t> limitTimeBase;
        std::map<std::string, uint64_t> goalTimeBase;

        //misc stuff
        std::map<std::string, std::string> appNamePid;
        std::map<std::string, std::string> pidAppName;
    };

    //Intended to hold data for max 1 whole day
    struct HistoricalData_Singular {
        std::map<std::string, uint64_t> timeLog_PerApp;
        std::map<std::string, uint64_t> timeLog_PerTab;
        std::map<std::string, uint64_t> timeLog_PerProject;
        std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;
        bool isLoaded = false;
    };

    struct HistoricalData_Full {
        std::map<std::string, uint64_t> timeLog_PerApp;
        std::map<std::string, uint64_t> timeLog_PerTab;
        std::map<std::string, uint64_t> timeLog_PerProject;
        std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;
        bool isLoaded = false;
    };

    struct TimelineEventInternal {
        double x;
        double width;
        std::string appName;
        std::string duration;
        std::string timeRange;
    };

    extern AppState state;
    extern HistoricalData_Singular historicalData_State; //keep name as is because i dont wanna update all files
    extern HistoricalData_Full historicalData_Full_State;

    extern ConfigManager configManager;
    extern const std::string APP_VERSION;

    extern AliasManager aliasManager;
    extern PatternAnalyzer patternAnalyzer;
    extern ThemeManager themeManager;

    extern ExtensionManager* extManager;

    extern std::mutex patternAnalyzerMutex;

    extern std::mutex stateMutex;
    extern std::mutex historyStateMutex;
    extern std::mutex historyLoadedMutex;
    extern std::condition_variable historyLoadedCV;

    extern std::vector<TimelineEventInternal> timelineEvents;
    extern std::mutex timelineMutex;

    struct TimelineMarkerInternal {
        double x;
        std::string label;
    };
    extern std::vector<TimelineMarkerInternal> timelineMarkers;
}

