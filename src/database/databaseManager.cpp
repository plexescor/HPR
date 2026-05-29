#include "databaseManager.hpp"
#include "timeUtils.hpp"
#include "appState.hpp"

#include "appEvents.hpp"

#include <sqlite3.h>//for extensinos ot execute sql
#include <sqlite_modern_cpp.h>

#include <chrono>
#include <iostream>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <filesystem>
#include <future>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/file.h>
#endif

DatabaseManager::DatabaseManager()
{

    initDatabase();

    // Try to create a lock file
    std::string lockPath = filePath + "hpr.lock";

    #ifdef _WIN32
        lockHandle = CreateFileA(
                lockPath.c_str(),
                GENERIC_WRITE,
                0,              // 0 = exclusive access
                NULL,
                OPEN_ALWAYS,    // Create if missing, open if exist, crucial if HPR crashed
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
                NULL
            );
            if (lockHandle == INVALID_HANDLE_VALUE) 
            {
                DWORD err = GetLastError();
                if (err == ERROR_SHARING_VIOLATION) 
                {
                    std::cerr << "[HPR] Already running. Exiting.\n";
                } 
                else 
                {
                    std::cerr << "[HPR] Could not acquire lock (Error " << err << ").\n";
                }
                exit(1);
            }
    #else
        int fd = open(lockPath.c_str(), O_RDWR | O_CREAT, 0644);
        if (fd == -1) {
            std::cerr << "[HPR] Failed to open lock file.\n";
            exit(1);
        }
        if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
            std::cerr << "[HPR] Already running. Exiting.\n";
            close(fd);
            exit(1);
        }
        lockFd = fd;
    #endif
    if (!loadStateFromDB())
    {
        std::cerr << "Failed to load data from db!\n";
    }

    //Connect to the event manager and get an id
    //Listen for load singular db file signal
    singular_DbLoadEventId = EventHub::connect(Event::LOAD_DATABASE_SINGULAR, [this](EventData data)
    {
        //If desired data exists
        if (std::holds_alternative<DatabaseDate_Singular>(data)) 
        {
            std::string requestedDate = std::get<DatabaseDate_Singular>(data).date;
            this->loadDb_Singular(requestedDate);
        }
    });
    
}

DatabaseManager::~DatabaseManager()
{
    running = false;
    if (writer.joinable()) writer.join();

    #ifdef _WIN32
        if (lockHandle != INVALID_HANDLE_VALUE) 
        {
            CloseHandle(lockHandle); // windows autodelete lock file
        }
    #else
        if (lockFd != -1) 
        {
            close(lockFd); // kernal will delete the lock file auto
        }
    #endif

    EventHub::disconnect(Event::LOAD_DATABASE_SINGULAR, singular_DbLoadEventId);
}

void DatabaseManager::initDatabase(bool copyData)
{

    updateFilePath();
    updateFileName();

    if (copyData)
    {
        //We need copies of the data for extra safety
        {
            std::lock_guard<std::mutex> lock(AppState::stateMutex);
            timeLog_PerApp_D = AppState::state.timeLog_PerApp;
            timeLog_PerTab_D = AppState::state.timeLog_PerTab;
            timeLog_PerProject_D = AppState::state.timeLog_PerProject;
            switchHistory_D = AppState::state.switchHistory;
        }
    }

    db.emplace(filePath + fileName);

    // WAL mode + synchronous normal — critical for Btrfs+LUKS
    *db << "PRAGMA journal_mode=WAL;";
    *db << "PRAGMA synchronous=NORMAL;";

    *db <<
         "create table if not exists app_usage ("
         "   name text unique,"
         "   duration int"
         ");";

    *db <<
        "create table if not exists tab_usage("
        "    name text unique,"
        "    duration int"
        ");";
    
    *db <<
        "create table if not exists project_usage("
        "    name text unique,"
        "    duration int"
        ");";

    *db <<
         "create table if not exists switch_history ("
         "   fromWindow text,"
         "   toWindow text,"
         "   timeStamp int unique"
         ");";
}

void DatabaseManager::run()
{
    writer = std::thread(&DatabaseManager::writeLoop, this);
}

bool DatabaseManager::loadStateFromDB()
{
    try
    {

        //No mutex needed because it runs before everything else
        
        // Load app_usage into AppState
        *db << "select name, duration from app_usage;"
        >> [](std::string name, long duration) {
            AppState::state.timeLog_PerApp[name] += duration;
        };

        *db << "select name, duration from tab_usage;"
        >> [](std::string name, long duration) {
            AppState::state.timeLog_PerTab[name] += duration;
        };

        *db << "select name, duration from project_usage;"
        >> [](std::string name, long duration) {
            AppState::state.timeLog_PerProject[name] += duration;
        };

        // Load switch_history into AppState  
        *db << "select fromWindow, toWindow, timeStamp from switch_history;"
        >> [](std::string from, std::string to, long long ts) {
            AppState::state.switchHistory[{from, to}].push_back((uint64_t)ts);
        };

    } catch(const std::exception& e)
    {
        std::cerr << "[ERROR IN DB LOAD FROM DISK] " << e.what() << std::endl;
        return false;
    }

    return true;
}

void DatabaseManager::writeLoop()
{
    while (running)
    {
        {
            std::lock_guard<std::mutex> dbLock(dbQueryMutex); // Lock database access for this write cycle
            *db << "BEGIN;";

            {
                //Copy fresh data
                std::lock_guard<std::mutex> lock(AppState::stateMutex);
                timeLog_PerApp_D = AppState::state.timeLog_PerApp;
                timeLog_PerTab_D = AppState::state.timeLog_PerTab;
                timeLog_PerProject_D = AppState::state.timeLog_PerProject;
                switchHistory_D = AppState::state.switchHistory;
            }

        for (const auto &[k, v] : timeLog_PerApp_D)
        {
            *db << "insert or replace into app_usage (name,duration) values (?,?);"
               << k
               << v;
        }

        for (const auto &[k, v] : timeLog_PerTab_D)
        {
            *db << "insert or replace into tab_usage (name,duration) values (?,?);"
               << k
               << v;
        }

        for (const auto &[k, v] : timeLog_PerProject_D)
        {
            *db << "insert or replace into project_usage (name,duration) values (?,?);"
               << k
               << v;
        }

        for (const auto &[k, v] : switchHistory_D)
        {
            //k = pair<>, v = vector<>
            const auto& [from, to] = k;

            for (const auto& timestamp : v)
            {
                *db << "insert or ignore into switch_history (fromWindow,toWindow,timeStamp) values (?,?,?);"
                << from
                << to
                << static_cast<long long>(timestamp);
            }
        }

        *db << "COMMIT;";
        *db << "PRAGMA wal_checkpoint(PASSIVE);"; //learnt hardway because wal itself can get corrupted during crashes
        //So that old db is closed and new file is created if date changes

        std::string newName = "";
        //Get ms sinc epoch
        auto nowSystem = std::chrono::system_clock::now();
        uint64_t t = std::chrono::duration_cast<std::chrono::milliseconds>(
            nowSystem.time_since_epoch()).count();

        newName += convertToDate_DDMMYY(t) + ".db";
        
        //Means if date has changed
        if (newName != fileName)
        {
            // Clear appstate in case of new day
            {
                std::lock_guard<std::mutex> lock(AppState::stateMutex);
                AppState::state.timeLog_PerApp.clear();
                AppState::state.timeLog_PerTab.clear();
                AppState::state.timeLog_PerProject.clear();
                AppState::state.switchHistory.clear();
            }
            //Emit a signal so extensions can also reset their daily data if they want to, and also to trigger the loading of the new db file if needed
            EventHub::emit(Event::MIDNIGHT_ROLLOVER);
            // so it doesnt copy the data of previous day or whatever
            initDatabase(false);

        }
        } // <-- Close the dbLock block so the lock is released during sleep!

        //Sleep in 100 chunks of 100ms each so program can exit almost instantly
        for (int i = 0; i < 100 && running; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

std::string DatabaseManager::getDbPathForDate(const std::string& date)
{
    std::string path;
    #ifdef _WIN32
        path = std::getenv("APPDATA");
        path += "/HPR/HPR_DB/" + extractMMYY_from_DDMMYY(date) + "/" + date + ".db";
    #else
        const char* home = std::getenv("HOME");
        if (!home) throw std::runtime_error("HOME env var not set");
        path = home;
        path += "/.local/share/HPR/HPR_DB/" + extractMMYY_from_DDMMYY(date) + "/" + date + ".db";
    #endif
    return path;
}

std::string DatabaseManager::getLoadedHistDbPath() const
{
    return loadedHistDbPath;
}

void DatabaseManager::loadDb_Singular(std::string requestedDate)
{
    //Create async task to load the singular db file
    historyLoadTask_Singular = std::async(std::launch::async, [this, requestedDate]() {
        try {

            //Get the filepath for desired files
            //DEVELOPER NOTES:
            //As its the free version, i therefore only allow loading of the db files created this month
            //of course you can modify the source code and give everyone the version that handles the month also
            //and i will not do anything, :)

            
            std::string path = getDbPathForDate(requestedDate);
            loadedHistDbPath = path;

            //Check if shit even exists
            if (!std::filesystem::exists(path))
            {
                EventHub::emit(Event::APP_ERROR, ErrorGui{"File not found! " + path});
                std::cerr << "Historical file not found: " << path << std::endl;
                return; 
            }

            //Load a new db
            sqlite::database histDb(path);

            //Force so incomplete .db files possibly due to a crash are fully 
            //resolved by a WAL checkpoint
            histDb << "PRAGMA wal_checkpoint(TRUNCATE);";

            //Create an intermediate map
            std::map<std::string, long> results;
            std::map<std::string, long> results_Tab;
            std::map<std::string, long> results_Project;
            std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> results_Switch;

            //load from db to the map
            histDb << "select name, duration from app_usage;"
                   >> [&results](std::string name, long duration) {
                       results[name] = duration;
                   };

            histDb << "select name, duration from tab_usage;"
                   >> [&results_Tab](std::string name, long duration) {
                       results_Tab[name] = duration;
                   };

            histDb << "select name, duration from project_usage;"
                   >> [&results_Project](std::string name, long duration) {
                       results_Project[name] = duration;
                   };
            
            histDb << "select fromWindow, toWindow, timeStamp from switch_history;"
                >> [&results_Switch](std::string from, std::string to, long long ts) {
                    results_Switch[{from, to}].push_back((uint64_t)ts);
                };
                

            //YOU: see appstate for this
            {
                std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
                AppState::historicalData_State.timeLog_PerApp = results;
                AppState::historicalData_State.timeLog_PerTab = results_Tab;
                AppState::historicalData_State.timeLog_PerProject = results_Project;
                AppState::historicalData_State.switchHistory = results_Switch;
                AppState::historicalData_State.isLoaded = true;
            }

            EventHub::emit(Event::HISTORY_LOADED_SINGULAR);
            
        } catch (const std::exception& e) {
            std::cerr << "Failed to load history: " << e.what() << std::endl;
        }
    });
}

// Runs INSERT, UPDATE, DELETE, CREATE TABLE, etc.
void DatabaseManager::executeSQL(const std::string& sql, const std::vector<std::string>& params)
{
    std::lock_guard<std::mutex> lock(dbQueryMutex);
    if (!db) return;
    try
    {
        auto binder = *db << sql;
        for (const auto& param : params)
        {
            binder << param;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[DB EXECUTE ERROR] " << e.what() << " | SQL: " << sql << std::endl;
    }
}

// Runs SELECT and returns dynamic columns and rows to Lua
std::vector<std::map<std::string, std::string>> DatabaseManager::querySQL(const std::string& sql, const std::vector<std::string>& params)
{
    std::lock_guard<std::mutex> lock(dbQueryMutex);
    std::vector<std::map<std::string, std::string>> results;
    if (!db) return results;
    // Get the raw sqlite3 connection handle from sqlite_modern_cpp
    sqlite3* rawDb = db->connection().get();
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(rawDb, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "[DB QUERY PREPARE ERROR] " << sqlite3_errmsg(rawDb) << " | SQL: " << sql << std::endl;
        return results;
    }

    // Bind parameters dynamically
    for (size_t i = 0; i < params.size(); i++)
    {
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
    }

    int colCount = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        std::map<std::string, std::string> row;
        for (int i = 0; i < colCount; i++)
        {
            const char* colName = sqlite3_column_name(stmt, i);
            const unsigned char* colVal = sqlite3_column_text(stmt, i);
            
            row[colName] = colVal ? reinterpret_cast<const char*>(colVal) : "";
        }
        results.push_back(row);
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<std::map<std::string, std::string>> DatabaseManager::querySQL_Path(const std::string& dbPath, const std::string& sql, const std::vector<std::string>& params)
{
    std::vector<std::map<std::string, std::string>> results;
    try
    {
        sqlite::database targetDb(dbPath);
        sqlite3* rawDb = targetDb.connection().get();
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(rawDb, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::cerr << "[DB QUERY PATH ERROR] " << sqlite3_errmsg(rawDb) << " | SQL: " << sql << std::endl;
            return results;
        }

        // Bind parameters dynamically
        for (size_t i = 0; i < params.size(); i++)
        {
            sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
        }

        int colCount = sqlite3_column_count(stmt);
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            std::map<std::string, std::string> row;
            for (int i = 0; i < colCount; i++)
            {
                const char* colName = sqlite3_column_name(stmt, i);
                const unsigned char* colVal = sqlite3_column_text(stmt, i);
                row[colName] = colVal ? reinterpret_cast<const char*>(colVal) : "";
            }
            results.push_back(row);
        }
        sqlite3_finalize(stmt);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[DB QUERY PATH EXCEPTION] " << e.what() << " | SQL: " << sql << std::endl;
    }
    return results;
}

void DatabaseManager::updateFilePath()
{
    //Now construct the path acc to the date

    //Get ms sinc epoch
    auto nowSystem = std::chrono::system_clock::now();
	uint64_t t = std::chrono::duration_cast<std::chrono::milliseconds>(
        nowSystem.time_since_epoch()).count();

    std::string tempPath;
    #ifdef _WIN32
        tempPath = std::getenv("APPDATA");
        tempPath += "/HPR/HPR_DB/" + convertToDate_MMYY(t) + "/";
    #else
        const char* home = std::getenv("HOME");
        if (!home) throw std::runtime_error("HOME env var not set");
        tempPath = home;
        tempPath += "/.local/share/HPR/HPR_DB/" + convertToDate_MMYY(t) + "/";
    #endif
    
    filePath = tempPath;
    
    // std::cout << filePath << std::endl;
    std::filesystem::create_directories(filePath);
}

void DatabaseManager::updateFileName()
{
    std::string tempName = "";
    //Get ms sinc epoch
    auto nowSystem = std::chrono::system_clock::now();
	uint64_t t = std::chrono::duration_cast<std::chrono::milliseconds>(
        nowSystem.time_since_epoch()).count();

    tempName += convertToDate_DDMMYY(t) + ".db";

    fileName = tempName;
    // std::cout << tempName << std::endl;
}