#include "appState.hpp"

// Do the shit here so it doesnt need to be done in a random cpp
AppState::AppState AppState::state;
AppState::HistoricalData AppState::historicalData_State;

AliasManager AppState::aliasManager;
PatternAnalyzer AppState::patternAnalyzer;

std::mutex AppState::patternAnalyzerMutex;

std::mutex AppState::stateMutex;
std::mutex AppState::historyStateMutex;
std::mutex AppState::historyLoadedMutex;
std::condition_variable AppState::historyLoadedCV;