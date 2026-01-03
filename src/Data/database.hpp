#ifndef DATABASE_HPP
#define DATABASE_HPP

#include "sqlite3.h"

// Struct for switch data
struct SwitchData {
    int id;
    char fromApp[256];
    char toApp[256];
    long long timestamp;
};

// Struct for app usage data
struct AppUsageData {
    int id;
    char appName[256];
    int totalDuration;
    int switchCount;
};

extern sqlite3* gDatabase;

// Setup
bool initializeDatabase(const char* dbPath);
void closeDatabase();
bool createTables();

// Table 1: Apps Usage (app name + duration)
bool insertAppUsage(const char* appName, int durationSeconds, int switchCount);
bool updateAppUsage(const char* appName, int durationSeconds, int switchCount);
int getAppUsage(const char* appName);
bool removeAppUsage(const char* appName);
AppUsageData* getAllAppUsage(int* count);  // NEW: Get all apps and their usage
void freeAppUsageData(AppUsageData* data); // NEW: Free memory

// Table 2: App Switches (from -> to)
int insertSwitch(const char* fromApp, const char* toApp);
bool removeSwitch(int id);
int getTotalSwitches();
SwitchData getSwitch(int id);                           // NEW: Get specific switch by ID
SwitchData* getAllSwitches(int* count);                 // NEW: Get all switches
SwitchData* getSwitchesFromApp(const char* appName, int* count);  // NEW: Get switches FROM an app
SwitchData* getSwitchesToApp(const char* appName, int* count);    // NEW: Get switches TO an app
void freeSwitchData(SwitchData* data);                  // NEW: Free memory

#endif