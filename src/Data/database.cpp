//------------------------------------------------------------------------------------------------------
//
//                 BETTER NOT MESS WITH THIS BORING AND VERBOSE SHIT, even i didnt, GPT did
//                                      BUT NEVERTHELESS, it works!
//
//------------------------------------------------------------------------------------------------------
//I didnt even bother to read this thoroughly, i read this once in my entire life, just so
// I could make wrappers for this nonsense, btw they are in diskWriteHandlerMain.cpp
//That file is understandable to the common man!
#include "database.hpp"
#include <iostream>
#include <cstring>
#include <chrono>
#include <cstdlib>

sqlite3* gDatabase = nullptr;

bool initializeDatabase(const char* dbPath) {
    int result = sqlite3_open(dbPath, &gDatabase);
    
    if (result != SQLITE_OK) {
        std::cerr << "Failed to open database" << std::endl;
        return false;
    }
    
    std::cout << "Database initialized" << std::endl;
    return true;
}

void closeDatabase() {
    if (gDatabase) {
        sqlite3_close(gDatabase);
        gDatabase = nullptr;
        std::cout << "Database closed" << std::endl;
    }
}

bool createTables() {
    if (!gDatabase) return false;
    
    // Table 1: App usage (app name + total duration)
    const char* createAppsTable = R"(
        CREATE TABLE IF NOT EXISTS apps_usage (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            appName TEXT UNIQUE NOT NULL,
            totalDuration INTEGER DEFAULT 0,
            switchCount INTEGER DEFAULT -1
        );
    )";
    
    // Table 2: App switches (from -> to)
    const char* createSwitchesTable = R"(
        CREATE TABLE IF NOT EXISTS app_switches (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            fromApp TEXT NOT NULL,
            toApp TEXT NOT NULL,
            timestamp INTEGER NOT NULL
        );
    )";
    
    char* errorMsg = nullptr;
    
    if (sqlite3_exec(gDatabase, createAppsTable, nullptr, nullptr, &errorMsg) != SQLITE_OK) {
        std::cerr << "Failed to create apps_usage table: " << errorMsg << std::endl;
        sqlite3_free(errorMsg);
        return false;
    }
    
    if (sqlite3_exec(gDatabase, createSwitchesTable, nullptr, nullptr, &errorMsg) != SQLITE_OK) {
        std::cerr << "Failed to create app_switches table: " << errorMsg << std::endl;
        sqlite3_free(errorMsg);
        return false;
    }
    
    std::cout << "Tables created" << std::endl;
    return true;
}

// ========== APP USAGE FUNCTIONS ==========

bool insertAppUsage(const char* appName, int durationSeconds, int switchCount) {
    if (!gDatabase) return false;
    
    const char* sql = R"(
        INSERT INTO apps_usage (appName, totalDuration, switchCount) VALUES (?, ?, ?)
        ON CONFLICT(appName) DO UPDATE SET totalDuration = excluded.totalDuration, switchCount = excluded.switchCount;
    )";
    
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(gDatabase, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement" << std::endl;
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, appName, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, durationSeconds);
    sqlite3_bind_int(stmt, 3, switchCount);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    if (success) {
        std::cout << "Set duration for " << appName << " to " << durationSeconds << "s" << std::endl;
    }
    
    return success;
}

bool updateAppUsage(const char* appName, int durationSeconds, int switchCount) {
    if (!gDatabase) return false;
    
    const char* sql = "UPDATE apps_usage SET totalDuration = ?, switchCount = ? WHERE appName = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(gDatabase, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, durationSeconds);
    sqlite3_bind_int(stmt, 2, switchCount);
    sqlite3_bind_text(stmt, 3, appName, -1, SQLITE_TRANSIENT);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    return success;
}

int getAppUsage(const char* appName) {
    if (!gDatabase) return 0;
    
    const char* sql = "SELECT totalDuration FROM apps_usage WHERE appName = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(gDatabase, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, appName, -1, SQLITE_TRANSIENT);
    
    int duration = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        duration = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return duration;
}

bool removeAppUsage(const char* appName) {
    if (!gDatabase) return false;
    
    const char* sql = "DELETE FROM apps_usage WHERE appName = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(gDatabase, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, appName, -1, SQLITE_TRANSIENT);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    return success;
}

// NEW: Get all app usage
AppUsageData* getAllAppUsage(int* count) {
    if (!gDatabase || !count) return nullptr;
    
    *count = 0;
    
    // First, count how many apps we have
    const char* countSql = "SELECT COUNT(*) FROM apps_usage;";
    sqlite3_stmt* countStmt;
    
    if (sqlite3_prepare_v2(gDatabase, countSql, -1, &countStmt, nullptr) != SQLITE_OK) {
        return nullptr;
    }
    
    if (sqlite3_step(countStmt) == SQLITE_ROW) {
        *count = sqlite3_column_int(countStmt, 0);
    }
    sqlite3_finalize(countStmt);
    
    if (*count == 0) return nullptr;
    
    // Allocate array
    AppUsageData* apps = (AppUsageData*)malloc(sizeof(AppUsageData) * (*count));
    if (!apps) return nullptr;
    
    // Get all apps
    const char* sql = "SELECT id, appName, totalDuration, switchCount FROM apps_usage ORDER BY totalDuration DESC;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(gDatabase, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        free(apps);
        return nullptr;
    }
    
    int index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && index < *count) {
        apps[index].id = sqlite3_column_int(stmt, 0);
        
        const char* name = (const char*)sqlite3_column_text(stmt, 1);
        if (name) {
            strncpy(apps[index].appName, name, sizeof(apps[index].appName) - 1);
            apps[index].appName[sizeof(apps[index].appName) - 1] = '\0';
        }
        
        apps[index].totalDuration = sqlite3_column_int(stmt, 2);
        apps[index].switchCount = sqlite3_column_int(stmt, 3);
        index++;
    }
    
    sqlite3_finalize(stmt);
    return apps;
}

void freeAppUsageData(AppUsageData* data) {
    if (data) {
        free(data);
    }
}

// ========== SWITCH FUNCTIONS ==========

int insertSwitch(const char* fromApp, const char* toApp) {
    if (!gDatabase) return -1;
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();
    
    const char* sql = "INSERT INTO app_switches (fromApp, toApp, timestamp) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(gDatabase, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, fromApp, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, toApp, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, timestamp);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }
    
    int newId = static_cast<int>(sqlite3_last_insert_rowid(gDatabase));
    sqlite3_finalize(stmt);
    
    std::cout << "Switch recorded: " << fromApp << " -> " << toApp << " (timestamp: " << timestamp << ")" << std::endl;
    return newId;
}

bool removeSwitch(int id) {
    if (!gDatabase) return false;
    
    const char* sql = "DELETE FROM app_switches WHERE id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(gDatabase, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    return success;
}

int getTotalSwitches() {
    if (!gDatabase) return 0;
    
    const char* sql = "SELECT COUNT(*) FROM app_switches;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(gDatabase, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// NEW: Get specific switch by ID
SwitchData getSwitch(int id) {
    SwitchData sw = {0};
    if (!gDatabase) return sw;
    
    const char* sql = "SELECT id, fromApp, toApp, timestamp FROM app_switches WHERE id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(gDatabase, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return sw;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        sw.id = sqlite3_column_int(stmt, 0);
        
        const char* from = (const char*)sqlite3_column_text(stmt, 1);
        if (from) {
            strncpy(sw.fromApp, from, sizeof(sw.fromApp) - 1);
            sw.fromApp[sizeof(sw.fromApp) - 1] = '\0';
        }
        
        const char* to = (const char*)sqlite3_column_text(stmt, 2);
        if (to) {
            strncpy(sw.toApp, to, sizeof(sw.toApp) - 1);
            sw.toApp[sizeof(sw.toApp) - 1] = '\0';
        }
        
        sw.timestamp = sqlite3_column_int64(stmt, 3);
    }
    
    sqlite3_finalize(stmt);
    return sw;
}

// NEW: Get all switches
SwitchData* getAllSwitches(int* count) {
    if (!gDatabase || !count) return nullptr;
    
    *count = 0;
    
    // Count switches
    const char* countSql = "SELECT COUNT(*) FROM app_switches;";
    sqlite3_stmt* countStmt;
    
    if (sqlite3_prepare_v2(gDatabase, countSql, -1, &countStmt, nullptr) != SQLITE_OK) {
        return nullptr;
    }
    
    if (sqlite3_step(countStmt) == SQLITE_ROW) {
        *count = sqlite3_column_int(countStmt, 0);
    }
    sqlite3_finalize(countStmt);
    
    if (*count == 0) return nullptr;
    
    // Allocate array
    SwitchData* switches = (SwitchData*)malloc(sizeof(SwitchData) * (*count));
    if (!switches) return nullptr;
    
    // Get all switches
    const char* sql = "SELECT id, fromApp, toApp, timestamp FROM app_switches ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(gDatabase, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        free(switches);
        return nullptr;
    }
    
    int index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && index < *count) {
        switches[index].id = sqlite3_column_int(stmt, 0);
        
        const char* from = (const char*)sqlite3_column_text(stmt, 1);
        if (from) {
            strncpy(switches[index].fromApp, from, sizeof(switches[index].fromApp) - 1);
            switches[index].fromApp[sizeof(switches[index].fromApp) - 1] = '\0';
        }
        
        const char* to = (const char*)sqlite3_column_text(stmt, 2);
        if (to) {
            strncpy(switches[index].toApp, to, sizeof(switches[index].toApp) - 1);
            switches[index].toApp[sizeof(switches[index].toApp) - 1] = '\0';
        }
        
        switches[index].timestamp = sqlite3_column_int64(stmt, 3);
        index++;
    }
    
    sqlite3_finalize(stmt);
    return switches;
}

// NEW: Get switches FROM a specific app
SwitchData* getSwitchesFromApp(const char* appName, int* count) {
    if (!gDatabase || !count) return nullptr;
    
    *count = 0;
    
    // Count switches
    const char* countSql = "SELECT COUNT(*) FROM app_switches WHERE fromApp = ?;";
    sqlite3_stmt* countStmt;
    
    if (sqlite3_prepare_v2(gDatabase, countSql, -1, &countStmt, nullptr) != SQLITE_OK) {
        return nullptr;
    }
    
    sqlite3_bind_text(countStmt, 1, appName, -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(countStmt) == SQLITE_ROW) {
        *count = sqlite3_column_int(countStmt, 0);
    }
    sqlite3_finalize(countStmt);
    
    if (*count == 0) return nullptr;
    
    // Allocate array
    SwitchData* switches = (SwitchData*)malloc(sizeof(SwitchData) * (*count));
    if (!switches) return nullptr;
    
    // Get switches
    const char* sql = "SELECT id, fromApp, toApp, timestamp FROM app_switches WHERE fromApp = ? ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(gDatabase, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        free(switches);
        return nullptr;
    }
    
    sqlite3_bind_text(stmt, 1, appName, -1, SQLITE_TRANSIENT);
    
    int index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && index < *count) {
        switches[index].id = sqlite3_column_int(stmt, 0);
        
        const char* from = (const char*)sqlite3_column_text(stmt, 1);
        if (from) {
            strncpy(switches[index].fromApp, from, sizeof(switches[index].fromApp) - 1);
            switches[index].fromApp[sizeof(switches[index].fromApp) - 1] = '\0';
        }
        
        const char* to = (const char*)sqlite3_column_text(stmt, 2);
        if (to) {
            strncpy(switches[index].toApp, to, sizeof(switches[index].toApp) - 1);
            switches[index].toApp[sizeof(switches[index].toApp) - 1] = '\0';
        }
        
        switches[index].timestamp = sqlite3_column_int64(stmt, 3);
        index++;
    }
    
    sqlite3_finalize(stmt);
    return switches;
}

// NEW: Get switches TO a specific app
SwitchData* getSwitchesToApp(const char* appName, int* count) {
    if (!gDatabase || !count) return nullptr;
    
    *count = 0;
    
    // Count switches
    const char* countSql = "SELECT COUNT(*) FROM app_switches WHERE toApp = ?;";
    sqlite3_stmt* countStmt;
    
    if (sqlite3_prepare_v2(gDatabase, countSql, -1, &countStmt, nullptr) != SQLITE_OK) {
        return nullptr;
    }
    
    sqlite3_bind_text(countStmt, 1, appName, -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(countStmt) == SQLITE_ROW) {
        *count = sqlite3_column_int(countStmt, 0);
    }
    sqlite3_finalize(countStmt);
    
    if (*count == 0) return nullptr;
    
    // Allocate array
    SwitchData* switches = (SwitchData*)malloc(sizeof(SwitchData) * (*count));
    if (!switches) return nullptr;
    
    // Get switches
    const char* sql = "SELECT id, fromApp, toApp, timestamp FROM app_switches WHERE toApp = ? ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(gDatabase, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        free(switches);
        return nullptr;
    }
    
    sqlite3_bind_text(stmt, 1, appName, -1, SQLITE_TRANSIENT);
    
    int index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && index < *count) {
        switches[index].id = sqlite3_column_int(stmt, 0);
        
        const char* from = (const char*)sqlite3_column_text(stmt, 1);
        if (from) {
            strncpy(switches[index].fromApp, from, sizeof(switches[index].fromApp) - 1);
            switches[index].fromApp[sizeof(switches[index].fromApp) - 1] = '\0';
        }
        
        const char* to = (const char*)sqlite3_column_text(stmt, 2);
        if (to) {
            strncpy(switches[index].toApp, to, sizeof(switches[index].toApp) - 1);
            switches[index].toApp[sizeof(switches[index].toApp) - 1] = '\0';
        }
        
        switches[index].timestamp = sqlite3_column_int64(stmt, 3);
        index++;
    }
    
    sqlite3_finalize(stmt);
    return switches;
}

void freeSwitchData(SwitchData* data) {
    if (data) {
        free(data);
    }
}