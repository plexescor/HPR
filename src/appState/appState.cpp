#include "appState.hpp"

// Do the shit here so it doesnt need to be done in a random cpp
AppState::AppState AppState::state;
AppState::HistoricalData_Singular AppState::historicalData_State;
AppState::HistoricalData_Full AppState::historicalData_Full_State;

AliasManager AppState::aliasManager;
ConfigManager AppState::configManager;
PatternAnalyzer AppState::patternAnalyzer;

ExtensionManager* AppState::extManager = nullptr;

std::mutex AppState::patternAnalyzerMutex;

std::mutex AppState::stateMutex;
std::mutex AppState::historyStateMutex;
std::mutex AppState::historyLoadedMutex;
std::condition_variable AppState::historyLoadedCV;

std::vector<AppState::TimelineEventInternal> AppState::timelineEvents;
std::mutex AppState::timelineMutex;

std::vector<AppState::TimelineMarkerInternal> AppState::timelineMarkers;