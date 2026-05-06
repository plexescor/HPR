#include "appState.hpp"

// Do the shit here so it doesnt need to be done in a random cpp
AppState::AppState AppState::state;
AppState::HistoricalData AppState::historicalData_State;

std::mutex AppState::stateMutex;
std::mutex AppState::historyStateMutex;