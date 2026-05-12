#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>

class PatternAnalyzer
{
    public:
        // PatternAnalyzer();
        // ~PatternAnalyzer();

        void generateInsights();

        std::string getMostUsed();
        std::string getTotalTrackedTime();
        std::string getSwitchCount();
        std::string getMostSwitchedFrom();
        std::string getMostSwitchedTo();
        std::string getMostFocusedSession();
        std::string getMostProductiveHour();


    private: //Local vars to hold mutex for little time
        std::map<std::string, long> timeLog_PerApp; 
        std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory; 
    
    private: //Vars used internally
        std::pair<std::string, int> mostSwitchedFrom;
        std::pair<std::string, int> mostSwitcedTo;
        std::pair<std::string, std::string> mostFocusedSession;
        std::string mostProductiveHour;

    private: //The actual insights output as a string
        //_O is "Output"
        std::string mostUsed_O;
        std::string totalTrackedTime_O;
        std::string switchCount_O;
        std::string mostSwitchedFrom_O;
        std::string mostSwitchedTo_O;
        std::string mostFocusedSession_O;
        std::string mostProductiveHour_O;
};