#pragma once

#include "database.hpp"

// Setup
bool initHandler(const char* dbPath);
void closeHandler();

// Write app usage (app name + duration in seconds)
bool writeAppUsage(const char* appName, int durationSeconds, int switchCount);

// Write switch (from app -> to app)
bool writeSwitch(const char* fromApp, const char* toApp);

// Read data
int readAppUsage(const char* appName);
int readTotalSwitches();
AppUsageData* readAllAppUsage(int* count);          // NEW
SwitchData* readAllSwitches(int* count);            // NEW
SwitchData* readSwitchesFrom(const char* appName, int* count);  // NEW
SwitchData* readSwitchesTo(const char* appName, int* count);    // NEW

// Delete data
bool deleteApp(const char* appName);
bool deleteSwitch(int id);