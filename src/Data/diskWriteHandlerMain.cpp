#include "database.hpp"
#include "diskWriteHandlerMain.hpp"
#include <iostream>

//--------------------------------------------------------------------------------------------------------
//
//                                      THIS FILE WORKS!
//
//--------------------------------------------------------------------------------------------------------
bool initHandler(const char* dbPath) {
    if (!initializeDatabase(dbPath)) {
        std::cerr << "Handler init failed" << std::endl;
        return false;
    }
    
    if (!createTables()) {
        std::cerr << "Table creation failed" << std::endl;
        return false;
    }
    
    std::cout << "Handler ready" << std::endl;
    return true;
}

void closeHandler() {
    closeDatabase();
}

bool writeAppUsage(const char* appName, int durationSeconds, int switchCount) {
    return insertAppUsage(appName, durationSeconds, switchCount);
}

bool writeSwitch(const char* fromApp, const char* toApp) {
    int id = insertSwitch(fromApp, toApp);
    return id != -1;
}

int readAppUsage(const char* appName) {
    return getAppUsage(appName);
}

AppUsageData* readAllAppUsage(int* count) {
    return getAllAppUsage(count);
} //Returns (for all entries) appName, duration (in s), and switchCount in form of struct

bool deleteApp(const char* appName) {
    return removeAppUsage(appName);
}

//Misleading name below !!!!!
SwitchData* readAllSwitches(int* count) {
    return getAllSwitches(count);
}
//---------------------------------NOT TO BE IMPLEMENTED NOW------------------------------------------------
// int readTotalSwitches() {
//     return getTotalSwitches();
// }



// SwitchData* readSwitchesFrom(const char* appName, int* count) {
//     return getSwitchesFromApp(appName, count);
// }

// SwitchData* readSwitchesTo(const char* appName, int* count) {
//     return getSwitchesToApp(appName, count);
// }

// bool deleteSwitch(int id) {
//     return removeSwitch(id);
// }