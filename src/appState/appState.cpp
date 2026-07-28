#include "appState.hpp"

// Do the shit here so it doesnt need to be done in a random cpp
AppState::AppState AppState::state;
AppState::HistoricalData_Singular AppState::historicalData_State;
AppState::HistoricalData_Full AppState::historicalData_Full_State;

AliasManager AppState::aliasManager;
ConfigManager AppState::configManager;
ThemeManager AppState::themeManager;

const std::string AppState::APP_VERSION = "0.9.6";
PatternAnalyzer AppState::patternAnalyzer;

ExtensionManager *AppState::extManager = nullptr;
LimitsManager *AppState::limitsManager = nullptr;

std::mutex AppState::patternAnalyzerMutex;

std::recursive_mutex AppState::stateMutex;
std::mutex AppState::historyStateMutex;
std::mutex AppState::historyLoadedMutex;
std::condition_variable AppState::historyLoadedCV;

std::vector<AppState::TimelineEventInternal> AppState::timelineEvents;
std::mutex AppState::timelineMutex;

std::vector<AppState::TimelineMarkerInternal> AppState::timelineMarkers;