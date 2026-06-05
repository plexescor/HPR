#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>

enum class AppCategory {
    WORK,
    SOCIAL,
    DISTRACTION,
    BROWSER,
    SYSTEM,
    UNKNOWN
};

struct switchHistory
{
    std::string date;
    std::string fromWindow;
    std::string toWindow;
    uint64_t timestamp;
};

// Holds one day of raw tracking data — used exclusively by generateAdvancedInsights()
struct DayData
{
    std::string date; // DDMMYY
    std::map<std::string, uint64_t> timePerApp;
    std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;
};


class PatternAnalyzer
{
    public:
        // PatternAnalyzer();
        // ~PatternAnalyzer();

        void generateInsights();
        void generateAdvancedInsights();
        AppCategory getCategory(const std::string appName);

        // Basic insight getters
        std::string getMostUsed();
        std::string getTotalTrackedTime();
        std::string getSwitchCount();
        std::string getMostSwitchedFrom();
        std::string getMostSwitchedTo();
        std::string getMostFocusedSession();
        std::string getMostProductiveHour();

        // Advanced cross-day insight getters
        std::string getEscapePattern();
        std::string getReturnRate();
        std::string getAvgFocusSession();
        std::string getMostDistractedDay();
        std::string getProductiveDaysThisWeek();
        std::string getScreenTimeVsAverage();
        std::string getFocusDipHour();
        std::string getDeepWorkBeforeNoon();
        std::string getWeekendVsWeekday();

        // Called by DatabaseManager when LOAD_PATTERNS_DATA fires
        void setMultiDayData(std::vector<DayData> data);


    private: //Local vars to hold mutex for little time
        std::map<std::string, uint64_t> timeLog_PerApp; 
        std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory; 

    private: //The actual insights output as a string
        //_O is "Output"
        std::string mostUsed_O;
        std::string totalTrackedTime_O;
        std::string switchCount_O;
        std::string mostSwitchedFrom_O;
        std::string mostSwitchedTo_O;
        std::string mostFocusedSession_O;
        std::string mostProductiveHour_O;

        //advanced stuff
        std::string escapePattern_O;
        std::string returnRate_O;
        std::string avgFocusSession_O;
        std::string mostDistractedDay_O;
        std::string productiveDaysThisWeek_O;
        std::string screenTimeVsAverage_O;
        std::string focusDipHour_O;
        std::string deepWorkBeforeNoon_O;
        std::string weekendVsWeekday_O;

    private: // Multi-day data loaded by DB manager
        std::vector<DayData> multiDayData_;
};