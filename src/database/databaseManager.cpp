#include "databaseManager.hpp"
#include "timeUtils.hpp"
#include "appState.hpp"
#include "extensionManager.hpp"
#include <cmath>
#include "appEvents.hpp"
#include "logger.hpp"
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
                    Logger::log("[HPR] Already running. Exiting.");
                } 
                else 
                {
                    std::cerr << "[HPR] Could not acquire lock (Error " << err << ").\n";
                    Logger::log("[HPR] Could not acquire lock (Error " + std::to_string(err) + ").");
                }
                exit(1);
            }
    #else
        int fd = open(lockPath.c_str(), O_RDWR | O_CREAT, 0644);
        if (fd == -1) 
        {
            std::cerr << "[HPR] Failed to open lock file.\n";
            Logger::log("[HPR] Failed to open lock file.");
            exit(1);
        }
        if (flock(fd, LOCK_EX | LOCK_NB) == -1) 
        {
            std::cerr << "[HPR] Already running. Exiting.\n";
            Logger::log("[HPR] Already running. Exiting.");
            close(fd);
            exit(1);
        }
        lockFd = fd;
    #endif
    if (!loadStateFromDB())
    {
        std::cerr << "Failed to load data from db!\n";
        Logger::log("Failed to load data from db!");
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

    number_DbLoadEventId = EventHub::connect(Event::LOAD_DATABASE_NUMBER, [this](EventData data)
    {
        if (std::holds_alternative<DatabaseDate_Number>(data)) 
        {
            auto arg = std::get<DatabaseDate_Number>(data);
            this->loadDb_Number(arg.days, arg.mode);
        }
    });
    
    range_DbLoadEventId = EventHub::connect(Event::LOAD_DATABASE_RANGE, [this](EventData data)
    {
        if (std::holds_alternative<DatabaseDate_Range>(data)) 
        {
            auto arg = std::get<DatabaseDate_Range>(data);
            this->loadDb_Range(arg.dateFrom, arg.dateTo, arg.mode);
        }
    });

    patterns_DbLoadEventId = EventHub::connect(Event::LOAD_PATTERNS_DATA, [this](EventData data)
    {
        if (std::holds_alternative<PatternDataRequest>(data))
        {
            int days = std::get<PatternDataRequest>(data).days;
            this->loadPatternsData(days);
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
    EventHub::disconnect(Event::LOAD_DATABASE_NUMBER, number_DbLoadEventId);
    EventHub::disconnect(Event::LOAD_DATABASE_RANGE, range_DbLoadEventId);
    EventHub::disconnect(Event::LOAD_PATTERNS_DATA, patterns_DbLoadEventId);
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
            appLimits_D = AppState::state.appLimits;
            appGoals_D = AppState::state.appGoals;
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

    *db <<
         "create table if not exists app_limits ("
         "   name text unique,"
         "   minutes int"
         ");";

    *db <<
         "create table if not exists app_goals ("
         "   name text unique,"
         "   minutes int"
         ");";

    *db << "create table if not exists limit_bases ("
     "   name text unique,"
     "   base_ms int"
     ");";

    *db << "create table if not exists goal_bases ("
        "   name text unique,"
        "   base_ms int"
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
        >> [](std::string name, uint64_t duration) {
            AppState::state.timeLog_PerApp[name] += duration;
        };

        *db << "select name, duration from tab_usage;"
        >> [](std::string name, uint64_t duration) {
            AppState::state.timeLog_PerTab[name] += duration;
        };

        *db << "select name, duration from project_usage;"
        >> [](std::string name, uint64_t duration) {
            AppState::state.timeLog_PerProject[name] += duration;
        };

        // Load switch_history into AppState  
        *db << "select fromWindow, toWindow, timeStamp from switch_history;"
        >> [](std::string from, std::string to, long long ts) {
            AppState::state.switchHistory[{from, to}].push_back((uint64_t)ts);
        };

        // Load app_limits & app_goals into AppState
        *db << "select name, minutes from app_limits;"
        >> [](std::string name, int minutes) {
            AppState::state.appLimits[name] = minutes;
        };

        *db << "select name, minutes from app_goals;"
        >> [](std::string name, int minutes) {
            AppState::state.appGoals[name] = minutes;
        };

        *db << "select name, base_ms from limit_bases;"
        >> [](std::string name, uint64_t base) {
            AppState::state.limitTimeBase[name] = base;
        };

        *db << "select name, base_ms from goal_bases;"
        >> [](std::string name, uint64_t base) {
            AppState::state.goalTimeBase[name] = base;
        };


    } catch(const std::exception& e)
    {
        std::cerr << "[ERROR IN DB LOAD FROM DISK] " << e.what() << std::endl;
        Logger::log("[ERROR IN DB LOAD FROM DISK] " + std::string(e.what()));
        return false;
    }

    return true;
}

void DatabaseManager::writeLoop()
{
    while (running)
    {
        bool needsRollover = false;
        std::string newName = "";

        {
            std::lock_guard<std::mutex> dbLock(dbQueryMutex);
            *db << "BEGIN;";

            {
                std::lock_guard<std::mutex> lock(AppState::stateMutex);
                timeLog_PerApp_D = AppState::state.timeLog_PerApp;
                timeLog_PerTab_D = AppState::state.timeLog_PerTab;
                timeLog_PerProject_D = AppState::state.timeLog_PerProject;
                switchHistory_D = AppState::state.switchHistory;
                appLimits_D = AppState::state.appLimits;
                appGoals_D = AppState::state.appGoals;
                limitTimeBase_D = AppState::state.limitTimeBase;
                goalTimeBase_D = AppState::state.goalTimeBase;
            }

            for (const auto &[k, v] : timeLog_PerApp_D)
                *db << "insert or replace into app_usage (name,duration) values (?,?);" << k << v;

            for (const auto &[k, v] : timeLog_PerTab_D)
                *db << "insert or replace into tab_usage (name,duration) values (?,?);" << k << v;

            for (const auto &[k, v] : timeLog_PerProject_D)
                *db << "insert or replace into project_usage (name,duration) values (?,?);" << k << v;

            for (const auto &[k, v] : switchHistory_D)
            {
                const auto& [from, to] = k;
                for (const auto& timestamp : v)
                    *db << "insert or ignore into switch_history (fromWindow,toWindow,timeStamp) values (?,?,?);"
                        << from << to << static_cast<long long>(timestamp);
            }

            *db << "delete from app_limits;";
            for (const auto &[k, v] : appLimits_D)
                if (v > 0)
                    *db << "insert or replace into app_limits (name,minutes) values (?,?);" << k << v;

            *db << "delete from app_goals;";
            for (const auto &[k, v] : appGoals_D)
                if (v > 0)
                    *db << "insert or replace into app_goals (name,minutes) values (?,?);" << k << v;

            *db << "delete from limit_bases;";
            for (const auto& [k, v] : limitTimeBase_D)
                *db << "insert or replace into limit_bases (name,base_ms) values (?,?);" << k << v;

            *db << "delete from goal_bases;";
            for (const auto& [k, v] : goalTimeBase_D)
                *db << "insert or replace into goal_bases (name,base_ms) values (?,?);" << k << v;

            std::vector<PendingQuery> queriesToRun;
            {
                std::lock_guard<std::mutex> lock(pendingQueriesMutex);
                queriesToRun = std::move(pendingQueries);
                pendingQueries.clear();
            }
            for (const auto& q : queriesToRun)
            {
                try
                {
                    auto binder = *db << q.sql;
                    for (const auto& param : q.params)
                        binder << param;
                }
                catch (const std::exception& e)
                {
                    std::cerr << "[DB QUEUED EXECUTE ERROR] " << e.what() << " | SQL: " << q.sql << std::endl;
                    Logger::log("[DB QUEUED EXECUTE ERROR] " + std::string(e.what()) + " | SQL: " + q.sql);
                }
            }

            *db << "COMMIT;";
            *db << "PRAGMA wal_checkpoint(PASSIVE);";

            
            auto nowSystem = std::chrono::system_clock::now();
            uint64_t t = std::chrono::duration_cast<std::chrono::milliseconds>(nowSystem.time_since_epoch()).count();
            newName = convertToDate_DDMMYY(t) + ".db";
            needsRollover = (newName != fileName);
        } 

        if (needsRollover)
        {
            {
                std::lock_guard<std::mutex> lock(AppState::stateMutex);
                AppState::state.timeLog_PerApp.clear();
                AppState::state.timeLog_PerTab.clear();
                AppState::state.timeLog_PerProject.clear();
                AppState::state.switchHistory.clear();
                AppState::state.limitTimeBase.clear();
                AppState::state.goalTimeBase.clear();
            }
            EventHub::emit(Event::MIDNIGHT_ROLLOVER);
            std::lock_guard<std::mutex> dbLock(dbQueryMutex);
            initDatabase(false);
        }

        int flushInterval = AppState::configManager.getConfig("db-flush-interval", 10000);
        int iterations = flushInterval / 100;
        if (iterations <= 0) iterations = 1;
        for (int i = 0; i < iterations && running; i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Final flush on shutdown
    std::vector<PendingQuery> queriesToRun;
    {
        std::lock_guard<std::mutex> lock(pendingQueriesMutex);
        queriesToRun = std::move(pendingQueries);
        pendingQueries.clear();
    }
    if (!queriesToRun.empty())
    {
        std::lock_guard<std::mutex> dbLock(dbQueryMutex);
        try
        {
            *db << "BEGIN;";
            for (const auto& q : queriesToRun)
            {
                auto binder = *db << q.sql;
                for (const auto& param : q.params)
                {
                    binder << param;
                }
            }
            *db << "COMMIT;";
            *db << "PRAGMA wal_checkpoint(PASSIVE);";
        }
        catch (const std::exception& e)
        {
            std::cerr << "[DB SHUTDOWN EXECUTE ERROR] " << e.what() << std::endl;
            Logger::log("[DB SHUTDOWN EXECUTE ERROR] " + std::string(e.what()));
        }
    }
}

std::string DatabaseManager::getDbPathForDate(const std::string& date)
{
    if (AppState::extManager)
    {
        auto res = AppState::extManager->dispatchOverride("getDbPathForDate", { CppValue(CppValue::Type::String, date) });
        if (res.has_value() && res->type == CppValue::Type::String)
        {
            return res->str_val;
        }
    }
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
    if (AppState::extManager)
    {
        auto res = AppState::extManager->dispatchOverride("getLoadedHistDbPath", {});
        if (res.has_value() && res->type == CppValue::Type::String)
        {
            return res->str_val;
        }
    }
    return loadedHistDbPath;
}

void DatabaseManager::loadDb_Singular(std::string requestedDate)
{
    // Reset the ready flag BEFORE launching async so waiters block correctly
    {
        std::lock_guard<std::mutex> lk(AppState::historyLoadedMutex);
        AppState::historicalData_State.isLoaded = false;
    }

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
                Logger::log("[HPR] Historical file not found: " + path);
                return; 
            }

            //Load a new db
            sqlite::database histDb(path);

            //Force so incomplete .db files possibly due to a crash are fully 
            //resolved by a WAL checkpoint
            histDb << "PRAGMA wal_checkpoint(TRUNCATE);";

            //Create an intermediate map
            std::map<std::string, uint64_t> results;
            std::map<std::string, uint64_t> results_Tab;
            std::map<std::string, uint64_t> results_Project;
            std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> results_Switch;

            //load from db to the map
            histDb << "select name, duration from app_usage;"
                   >> [&results](std::string name, uint64_t duration) {
                       results[name] = duration;
                   };

            histDb << "select name, duration from tab_usage;"
                   >> [&results_Tab](std::string name, uint64_t duration) {
                       results_Tab[name] = duration;
                   };

            histDb << "select name, duration from project_usage;"
                   >> [&results_Project](std::string name, uint64_t duration) {
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
            // Wake up any thread blocked inside dbQueryHistorical_E
            AppState::historyLoadedCV.notify_all();

            EventHub::emit(Event::HISTORY_LOADED_SINGULAR);
            
        } catch (const std::exception& e) {
            std::cerr << "Failed to load history: " << e.what() << std::endl;
            Logger::log("[HPR] Failed to load history: " + std::string(e.what()));
        }
    });
}

void DatabaseManager::loadDb_Number(int days, std::string mode)
{
    // Reset the ready flag BEFORE launching async so waiters block correctly
    {
        std::lock_guard<std::mutex> lk(AppState::historyLoadedMutex);
        AppState::historicalData_Full_State.isLoaded = false;
    }

    historyLoadTask_Number = std::async(std::launch::async, [this, days, mode]() {
        //total mode
        //get todays time stamp
        uint64_t timeStampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()
                            ).count();
        
        std::string activeFileName = this->fileName;

        if (mode == "total")
        {
            std::string path;
            #ifdef _WIN32
                path = std::getenv("APPDATA");
                path += "/HPR/HPR_DB/";
            #else
                const char* home = std::getenv("HOME");
                if (!home) throw std::runtime_error("HOME env var not set");
                path = home;
                path += "/.local/share/HPR/HPR_DB/";
            #endif

            //the file names to load
            std::vector<std::string> fileNames;
            fileNames.reserve(days);

            std::vector<std::thread> workers;
            workers.reserve(days);

            {
                std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
                AppState::historicalData_Full_State.timeLog_PerApp.clear();
                AppState::historicalData_Full_State.timeLog_PerTab.clear();
                AppState::historicalData_Full_State.timeLog_PerProject.clear();
                AppState::historicalData_Full_State.switchHistory.clear();
            }

            for (int i = 0; i < days; ++i)
            {
                std::string dateStr = convertToDate_DDMMYY(timeStampMs);
                fileNames.push_back(dateStr);
                timeStampMs = (timeStampMs >= 86400000) ? (timeStampMs - 86400000) : 0;
            }

            for (int i = 0; i < days; ++i)
            {
                workers.emplace_back([this, i, &fileNames, &path, activeFileName]()
                {
                    try
                    {
                        //Create intermediate maps
                        std::map<std::string, uint64_t> results;
                        std::map<std::string, uint64_t> results_Tab;
                        std::map<std::string, uint64_t> results_Project;
                        std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> results_Switch;

                        if (fileNames[i] + ".db" == activeFileName)
                        {
                            std::lock_guard<std::mutex> lock(AppState::stateMutex);
                            results = AppState::state.timeLog_PerApp;
                            results_Tab = AppState::state.timeLog_PerTab;
                            results_Project = AppState::state.timeLog_PerProject;
                            results_Switch = AppState::state.switchHistory;
                        }
                        else
                        {
                            std::string fullPath = path + extractMMYY_from_DDMMYY(fileNames[i]) + "/" + fileNames[i] + ".db";

                            if (!std::filesystem::exists(fullPath))
                            {
                                std::cerr << "Historical file not found: " << fullPath << std::endl;
                                Logger::log("[HPR] Historical file not found: " + fullPath);
                                return;
                            }

                            sqlite::database numDb(fullPath);

                            //load from db to the map
                            numDb << "select name, duration from app_usage;"
                                >> [&results](std::string name, uint64_t duration) {
                                    results[name] = duration;
                                };

                            numDb << "select name, duration from tab_usage;"
                                >> [&results_Tab](std::string name, uint64_t duration) {
                                    results_Tab[name] = duration;
                                };

                            numDb << "select name, duration from project_usage;"
                                >> [&results_Project](std::string name, uint64_t duration) {
                                    results_Project[name] = duration;
                                };
                            
                            numDb << "select fromWindow, toWindow, timeStamp from switch_history;"
                                >> [&results_Switch](std::string from, std::string to, long long ts) {
                                    results_Switch[{from, to}].push_back((uint64_t)ts);
                                };
                        }
                            
                        {
                            std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
                            for (const auto& [key, value] : results) 
                            {
                                AppState::historicalData_Full_State.timeLog_PerApp[key] += value;
                            }
                            for (const auto& [key, value] : results_Tab) 
                            {
                                AppState::historicalData_Full_State.timeLog_PerTab[key] += value;
                            }
                            for (const auto& [key, value] : results_Project) 
                            {
                                AppState::historicalData_Full_State.timeLog_PerProject[key] += value;
                            }
                            for (const auto& [key, value] : results_Switch) 
                            {
                                AppState::historicalData_Full_State.switchHistory[key].insert(
                                    AppState::historicalData_Full_State.switchHistory[key].end(), 
                                    value.begin(), 
                                    value.end()
                                );
                            }
                            AppState::historicalData_Full_State.isLoaded = true;
                        }

                    } catch (const std::exception& e) {
                        std::cerr << "Failed to load history: " << e.what() << std::endl;
                        Logger::log("[HPR] Failed to load history: " + std::string(e.what()));
                    }
                });
            }

            for (auto& w : workers)
            {
                if (w.joinable())
                    w.join();
            }

            EventHub::emit(Event::HISTORY_LOADED_NUMBER);
            
        }

        if (mode == "average")
        {
            std::string path;
            #ifdef _WIN32
                path = std::getenv("APPDATA");
                path += "/HPR/HPR_DB/";
            #else
                const char* home = std::getenv("HOME");
                if (!home) throw std::runtime_error("HOME env var not set");
                path = home;
                path += "/.local/share/HPR/HPR_DB/";
            #endif

            std::vector<std::string> fileNames;
            fileNames.reserve(days);

            for (int i = 0; i < days; ++i)
            {
                fileNames.push_back(convertToDate_DDMMYY(timeStampMs));
                timeStampMs = (timeStampMs >= 86400000) ? (timeStampMs - 86400000) : 0;
            }

            std::vector<std::map<std::string, uint64_t>> allApp(days);
            std::vector<std::map<std::string, uint64_t>> allTab(days);
            std::vector<std::map<std::string, uint64_t>> allProject(days);
            std::vector<std::map<std::pair<std::string, std::string>, std::vector<uint64_t>>> allSwitches(days);
            std::vector<bool> loaded(days, false);

            std::vector<std::thread> workers;
            workers.reserve(days);

            {
                std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
                AppState::historicalData_Full_State.timeLog_PerApp.clear();
                AppState::historicalData_Full_State.timeLog_PerTab.clear();
                AppState::historicalData_Full_State.timeLog_PerProject.clear();
                AppState::historicalData_Full_State.switchHistory.clear();
                AppState::historicalData_Full_State.isLoaded = false;
            }

            for (int i = 0; i < days; ++i)
            {
                workers.emplace_back([i, &fileNames, &path, &allApp, &allTab, &allProject, &allSwitches, &loaded, activeFileName]()
                {
                    try
                    {
                        if (fileNames[i] + ".db" == activeFileName)
                        {
                            std::lock_guard<std::mutex> lock(AppState::stateMutex);
                            allApp[i] = AppState::state.timeLog_PerApp;
                            allTab[i] = AppState::state.timeLog_PerTab;
                            allProject[i] = AppState::state.timeLog_PerProject;
                            allSwitches[i] = AppState::state.switchHistory;
                            loaded[i] = true;
                        }
                        else
                        {
                            std::string fullPath = path + extractMMYY_from_DDMMYY(fileNames[i]) + "/" + fileNames[i] + ".db";

                            if (!std::filesystem::exists(fullPath))
                            {
                                std::cerr << "Historical file not found: " << fullPath << std::endl;
                                Logger::log("[HPR] Historical file not found: " + fullPath);
                                return;
                            }

                            sqlite::database numDb(fullPath);

                            numDb << "select name, duration from app_usage;"
                                >> [&, i](std::string name, uint64_t duration) {
                                    allApp[i][name] = duration;
                                };

                            numDb << "select name, duration from tab_usage;"
                                >> [&, i](std::string name, uint64_t duration) {
                                    allTab[i][name] = duration;
                                };

                            numDb << "select name, duration from project_usage;"
                                >> [&, i](std::string name, uint64_t duration) {
                                    allProject[i][name] = duration;
                                };

                            numDb << "select fromWindow, toWindow, timeStamp from switch_history;"
                                >> [&, i](std::string from, std::string to, long long ts) {
                                    allSwitches[i][{from, to}].push_back((uint64_t)ts);
                                };

                            loaded[i] = true;
                        }

                    } catch (const std::exception& e) {
                        std::cerr << "Failed to load history: " << e.what() << std::endl;
                        Logger::log("[HPR] Failed to load history: " + std::string(e.what()));
                    }
                });
            }

            for (auto& w : workers)
                if (w.joinable()) w.join();

            // O(n) average pass
            std::map<std::string, uint64_t> totalApp;
            std::map<std::string, uint64_t> totalTab;
            std::map<std::string, uint64_t> totalProject;
            std::map<std::string, int> countApp;
            std::map<std::string, int> countTab;
            std::map<std::string, int> countProject;
            std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> totalSwitches;
            std::map<std::pair<std::string, std::string>, int> countSwitches;

            for (int i = 0; i < days; ++i)
            {
                if (!loaded[i]) continue;
                for (const auto& [k, v] : allApp[i])     { totalApp[k] += v;     countApp[k]++; }
                for (const auto& [k, v] : allTab[i])     { totalTab[k] += v;     countTab[k]++; }
                for (const auto& [k, v] : allProject[i]) { totalProject[k] += v; countProject[k]++; }
                for (const auto& [k, v] : allSwitches[i])
                {
                    totalSwitches[k].insert(totalSwitches[k].end(), v.begin(), v.end());
                    countSwitches[k]++;
                }
            }

            {
                std::lock_guard<std::mutex> lock(AppState::historyStateMutex);

                for (const auto& [k, v] : totalApp)
                    AppState::historicalData_Full_State.timeLog_PerApp[k] = v / countApp[k];

                for (const auto& [k, v] : totalTab)
                    AppState::historicalData_Full_State.timeLog_PerTab[k] = v / countTab[k];

                for (const auto& [k, v] : totalProject)
                    AppState::historicalData_Full_State.timeLog_PerProject[k] = v / countProject[k];

                for (const auto& [k, v] : totalSwitches)
                    AppState::historicalData_Full_State.switchHistory[k] = v;

                AppState::historicalData_Full_State.isLoaded = true;
            }

            EventHub::emit(Event::HISTORY_LOADED_NUMBER);
        }
    });
}

void DatabaseManager::loadDb_Range(std::string dateFrom, std::string dateTo, std::string mode)
{
    // Reset the ready flag BEFORE launching async so waiters block correctly
    {
        std::lock_guard<std::mutex> lk(AppState::historyLoadedMutex);
        AppState::historicalData_Full_State.isLoaded = false;
    }

    historyLoadTask_Range = std::async(std::launch::async, [this, dateFrom, dateTo, mode]() {
        int days = 0;
        //get time stamp of current date
        //context "dateFrom" the date far-er from present
        uint64_t timeStampFrom = parseDate_DDMMYY(dateFrom);
        uint64_t timeStampTo = parseDate_DDMMYY(dateTo);

        if (timeStampFrom == 0 || timeStampTo == 0)
        {
            std::cerr << "Invalid date format! Dates should be in DDMMYY format." << std::endl;
            Logger::log("[HPR] Invalid date format! Dates should be in DDMMYY format.");
            return;
        }

        //if the user reversed the order of dates
        if (timeStampFrom > timeStampTo)
        {
            timeStampFrom = parseDate_DDMMYY(dateTo);
            timeStampTo = parseDate_DDMMYY(dateFrom);
        }

        //finding total number of days between the two dates
        days = (timeStampTo - timeStampFrom) / 86400000 + 1; // +1 to include both start and end date
        
        std::string activeFileName = this->fileName;

        if (mode == "total")
        {
            std::string path;
            #ifdef _WIN32
                path = std::getenv("APPDATA");
                path += "/HPR/HPR_DB/";
            #else
                const char* home = std::getenv("HOME");
                if (!home) throw std::runtime_error("HOME env var not set");
                path = home;
                path += "/.local/share/HPR/HPR_DB/";
            #endif

            //the file names to load
            std::vector<std::string> fileNames;
            fileNames.reserve(days);

            std::vector<std::thread> workers;
            workers.reserve(days);

            {
                std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
                AppState::historicalData_Full_State.timeLog_PerApp.clear();
                AppState::historicalData_Full_State.timeLog_PerTab.clear();
                AppState::historicalData_Full_State.timeLog_PerProject.clear();
                AppState::historicalData_Full_State.switchHistory.clear();
            }

            for (int i = 0; i < days; ++i)
            {
                std::string dateStr = convertToDate_DDMMYY(timeStampTo);
                fileNames.push_back(dateStr);
                timeStampTo = (timeStampTo >= 86400000) ? (timeStampTo - 86400000) : 0;
            }

            for (int i = 0; i < days; ++i)
            {
                workers.emplace_back([this, i, &fileNames, &path, activeFileName]()
                {
                    try
                    {
                        //Create intermediate maps
                        std::map<std::string, uint64_t> results;
                        std::map<std::string, uint64_t> results_Tab;
                        std::map<std::string, uint64_t> results_Project;
                        std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> results_Switch;

                        if (fileNames[i] + ".db" == activeFileName)
                        {
                            std::lock_guard<std::mutex> lock(AppState::stateMutex);
                            results = AppState::state.timeLog_PerApp;
                            results_Tab = AppState::state.timeLog_PerTab;
                            results_Project = AppState::state.timeLog_PerProject;
                            results_Switch = AppState::state.switchHistory;
                        }
                        else
                        {
                            std::string fullPath = path + extractMMYY_from_DDMMYY(fileNames[i]) + "/" + fileNames[i] + ".db";

                            if (!std::filesystem::exists(fullPath))
                            {
                                std::cerr << "Historical file not found: " << fullPath << std::endl;
                                Logger::log("[HPR] Historical file not found: " + fullPath);
                                return;
                            }

                            sqlite::database numDb(fullPath);

                            //load from db to the map
                            numDb << "select name, duration from app_usage;"
                                >> [&results](std::string name, uint64_t duration) {
                                    results[name] = duration;
                                };

                            numDb << "select name, duration from tab_usage;"
                                >> [&results_Tab](std::string name, uint64_t duration) {
                                    results_Tab[name] = duration;
                                };

                            numDb << "select name, duration from project_usage;"
                                >> [&results_Project](std::string name, uint64_t duration) {
                                    results_Project[name] = duration;
                                };
                            
                            numDb << "select fromWindow, toWindow, timeStamp from switch_history;"
                                >> [&results_Switch](std::string from, std::string to, long long ts) {
                                    results_Switch[{from, to}].push_back((uint64_t)ts);
                                };
                        }
                            
                        {
                            std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
                            for (const auto& [key, value] : results) 
                            {
                                AppState::historicalData_Full_State.timeLog_PerApp[key] += value;
                            }
                            for (const auto& [key, value] : results_Tab) 
                            {
                                AppState::historicalData_Full_State.timeLog_PerTab[key] += value;
                            }
                            for (const auto& [key, value] : results_Project) 
                            {
                                AppState::historicalData_Full_State.timeLog_PerProject[key] += value;
                            }
                            for (const auto& [key, value] : results_Switch) 
                            {
                                AppState::historicalData_Full_State.switchHistory[key].insert(
                                    AppState::historicalData_Full_State.switchHistory[key].end(), 
                                    value.begin(), 
                                    value.end()
                                );
                            }
                            AppState::historicalData_Full_State.isLoaded = true;
                        }

                    } catch (const std::exception& e) {
                        std::cerr << "Failed to load history: " << e.what() << std::endl;
                        Logger::log("[HPR] Failed to load history: " + std::string(e.what()));
                    }
                });
            }

            for (auto& w : workers)
            {
                if (w.joinable())
                    w.join();
            }
            EventHub::emit(Event::HISTORY_LOADED_RANGE);
        }

        if (mode == "average")
        {
            std::string path;
            #ifdef _WIN32
                path = std::getenv("APPDATA");
                path += "/HPR/HPR_DB/";
            #else
                const char* home = std::getenv("HOME");
                if (!home) throw std::runtime_error("HOME env var not set");
                path = home;
                path += "/.local/share/HPR/HPR_DB/";
            #endif

            std::vector<std::string> fileNames;
            fileNames.reserve(days);

            for (int i = 0; i < days; ++i)
            {
                fileNames.push_back(convertToDate_DDMMYY(timeStampTo));
                timeStampTo = (timeStampTo >= 86400000) ? (timeStampTo - 86400000) : 0;
            }

            std::vector<std::map<std::string, uint64_t>> allApp(days);
            std::vector<std::map<std::string, uint64_t>> allTab(days);
            std::vector<std::map<std::string, uint64_t>> allProject(days);
            std::vector<std::map<std::pair<std::string, std::string>, std::vector<uint64_t>>> allSwitches(days);
            std::vector<bool> loaded(days, false);

            std::vector<std::thread> workers;
            workers.reserve(days);

            {
                std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
                AppState::historicalData_Full_State.timeLog_PerApp.clear();
                AppState::historicalData_Full_State.timeLog_PerTab.clear();
                AppState::historicalData_Full_State.timeLog_PerProject.clear();
                AppState::historicalData_Full_State.switchHistory.clear();
                AppState::historicalData_Full_State.isLoaded = false;
            }

            for (int i = 0; i < days; ++i)
            {
                workers.emplace_back([i, &fileNames, &path, &allApp, &allTab, &allProject, &allSwitches, &loaded, activeFileName]()
                {
                    try
                    {
                        if (fileNames[i] + ".db" == activeFileName)
                        {
                            std::lock_guard<std::mutex> lock(AppState::stateMutex);
                            allApp[i] = AppState::state.timeLog_PerApp;
                            allTab[i] = AppState::state.timeLog_PerTab;
                            allProject[i] = AppState::state.timeLog_PerProject;
                            allSwitches[i] = AppState::state.switchHistory;
                            loaded[i] = true;
                        }
                        else
                        {
                            std::string fullPath = path + extractMMYY_from_DDMMYY(fileNames[i]) + "/" + fileNames[i] + ".db";

                            if (!std::filesystem::exists(fullPath))
                            {
                                std::cerr << "Historical file not found: " << fullPath << std::endl;
                                Logger::log("[HPR] Historical file not found: " + fullPath);
                                return;
                            }

                            sqlite::database numDb(fullPath);

                            numDb << "select name, duration from app_usage;"
                                >> [&, i](std::string name, uint64_t duration) {
                                    allApp[i][name] = duration;
                                };

                            numDb << "select name, duration from tab_usage;"
                                >> [&, i](std::string name, uint64_t duration) {
                                    allTab[i][name] = duration;
                                };

                            numDb << "select name, duration from project_usage;"
                                >> [&, i](std::string name, uint64_t duration) {
                                    allProject[i][name] = duration;
                                };

                            numDb << "select fromWindow, toWindow, timeStamp from switch_history;"
                                >> [&, i](std::string from, std::string to, long long ts) {
                                    allSwitches[i][{from, to}].push_back((uint64_t)ts);
                                };

                            loaded[i] = true;
                        }

                    } catch (const std::exception& e) {
                        std::cerr << "Failed to load history: " << e.what() << std::endl;
                        Logger::log("[HPR] Failed to load history: " + std::string(e.what()));
                    }
                });
            }

            for (auto& w : workers)
                if (w.joinable()) w.join();

            // O(n) average pass
            std::map<std::string, uint64_t> totalApp;
            std::map<std::string, uint64_t> totalTab;
            std::map<std::string, uint64_t> totalProject;
            std::map<std::string, int> countApp;
            std::map<std::string, int> countTab;
            std::map<std::string, int> countProject;
            std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> totalSwitches;
            std::map<std::pair<std::string, std::string>, int> countSwitches;

            for (int i = 0; i < days; ++i)
            {
                if (!loaded[i]) continue;
                for (const auto& [k, v] : allApp[i])     { totalApp[k] += v;     countApp[k]++; }
                for (const auto& [k, v] : allTab[i])     { totalTab[k] += v;     countTab[k]++; }
                for (const auto& [k, v] : allProject[i]) { totalProject[k] += v; countProject[k]++; }
                for (const auto& [k, v] : allSwitches[i])
                {
                    totalSwitches[k].insert(totalSwitches[k].end(), v.begin(), v.end());
                    countSwitches[k]++;
                }
            }

            {
                std::lock_guard<std::mutex> lock(AppState::historyStateMutex);

                for (const auto& [k, v] : totalApp)
                    AppState::historicalData_Full_State.timeLog_PerApp[k] = v / countApp[k];

                for (const auto& [k, v] : totalTab)
                    AppState::historicalData_Full_State.timeLog_PerTab[k] = v / countTab[k];

                for (const auto& [k, v] : totalProject)
                    AppState::historicalData_Full_State.timeLog_PerProject[k] = v / countProject[k];

                for (const auto& [k, v] : totalSwitches)
                    AppState::historicalData_Full_State.switchHistory[k] = v;

                AppState::historicalData_Full_State.isLoaded = true;
            }

            EventHub::emit(Event::HISTORY_LOADED_RANGE);
        }
    });
}

// Synchronous: loads N days of per-day data into PatternAnalyzer for advanced insight computation.
// Called directly on the EventHub thread — no async needed because the UI just reads the output strings.
void DatabaseManager::loadPatternsData(int days)
{
    std::string activeFileName = this->fileName;

    std::string basePath;
    #ifdef _WIN32
        basePath = std::getenv("APPDATA");
        basePath += "/HPR/HPR_DB/";
    #else
        const char* home = std::getenv("HOME");
        if (!home)
        {
            std::cerr << "[loadPatternsData] HOME env var not set\n";
            Logger::log("[loadPatternsData] HOME env var not set");
            return;
        }
        basePath = home;
        basePath += "/.local/share/HPR/HPR_DB/";
    #endif

    // Walk backwards from today, one day at a time
    uint64_t tsMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();

    std::vector<DayData> result;
    result.reserve(days);

    for (int i = 0; i < days; ++i)
    {
        std::string dateStr = convertToDate_DDMMYY(tsMs);
        tsMs = (tsMs >= 86400000ULL) ? (tsMs - 86400000ULL) : 0;

        DayData dayData;
        dayData.date = dateStr;

        if (dateStr + ".db" == activeFileName)
        {
            // Today's live data — read from AppState under lock
            std::lock_guard<std::mutex> lock(AppState::stateMutex);
            dayData.timePerApp   = AppState::state.timeLog_PerApp;
            dayData.switchHistory = AppState::state.switchHistory;
        }
        else
        {
            std::string fullPath = basePath + extractMMYY_from_DDMMYY(dateStr) + "/" + dateStr + ".db";
            if (!std::filesystem::exists(fullPath))
            {
                // Missing day — skip silently (user may not have data for every day)
                continue;
            }

            try
            {
                sqlite::database dayDb(fullPath);
                dayDb << "PRAGMA wal_checkpoint(PASSIVE);";

                dayDb << "select name, duration from app_usage;"
                      >> [&dayData](std::string name, uint64_t duration) {
                             dayData.timePerApp[name] = duration;
                         };

                dayDb << "select fromWindow, toWindow, timeStamp from switch_history;"
                      >> [&dayData](std::string from, std::string to, long long ts) {
                             dayData.switchHistory[{from, to}].push_back((uint64_t)ts);
                         };
            }
            catch (const std::exception& e)
            {
                std::cerr << "[loadPatternsData] Failed to load " << fullPath << ": " << e.what() << "\n";
                Logger::log("[loadPatternsData] Failed to load " + fullPath + ": " + std::string(e.what()));
                continue;
            }
        }

        result.push_back(std::move(dayData));
    }

    {
        std::lock_guard<std::mutex> lock(AppState::patternAnalyzerMutex);
        AppState::patternAnalyzer.setMultiDayData(std::move(result));
        AppState::patternAnalyzer.generateAdvancedInsights();
    }
}

// Runs INSERT, UPDATE, DELETE, CREATE TABLE, etc.
void DatabaseManager::executeSQL(const std::string& sql, const std::vector<std::string>& params)
{
    if (AppState::extManager)
    {
        std::vector<CppValue> cppParams;
        for (const auto& p : params)
        {
            cppParams.push_back(CppValue(CppValue::Type::String, p));
        }
        CppValue paramsVal(CppValue::Type::Array);
        paramsVal.array_val = cppParams;

        auto res = AppState::extManager->dispatchOverride("dbExecute", { CppValue(CppValue::Type::String, sql), paramsVal });
        if (res.has_value())
        {
            return;
        }
    }
    std::lock_guard<std::mutex> lock(pendingQueriesMutex);
    pendingQueries.push_back({ sql, params });
}

ParallelQueryResult DatabaseManager::dbQueryNumber(int days, const std::string& mode, const std::string& sql, const std::vector<std::string>& params)
{
    ParallelQueryResult out;
    std::mutex localMutex; // local to this call only
    std::vector<std::map<std::string, std::string>> rawRows;

    uint64_t timeStampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::string activeFileName = this->fileName;

    std::string path;
    #ifdef _WIN32
        path = std::getenv("APPDATA");
        path += "/HPR/HPR_DB/";
    #else
        const char* home = std::getenv("HOME");
        if (!home) throw std::runtime_error("HOME env var not set");
        path = home;
        path += "/.local/share/HPR/HPR_DB/";
    #endif

    std::vector<std::string> fileNames;
    fileNames.reserve(days);
    for (int i = 0; i < days; ++i)
    {
        fileNames.push_back(convertToDate_DDMMYY(timeStampMs));
        timeStampMs = (timeStampMs >= 86400000) ? (timeStampMs - 86400000) : 0;
    }

    std::vector<std::thread> workers;
    workers.reserve(days);

    for (int i = 0; i < days; ++i)
    {
        workers.emplace_back([this, i, &fileNames, &path, activeFileName, &sql, &params, &localMutex, &rawRows]()
        {
            try
            {
                std::vector<std::map<std::string, std::string>> dayRows;

                if (fileNames[i] + ".db" == activeFileName)
                {
                    std::lock_guard<std::mutex> lock(dbQueryMutex);
                    if (db)
                    {
                        sqlite3* rawDb = db->connection().get();
                        sqlite3_stmt* stmt = nullptr;
                        if (sqlite3_prepare_v2(rawDb, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
                        {
                            for (size_t p = 0; p < params.size(); ++p)
                                sqlite3_bind_text(stmt, p + 1, params[p].c_str(), -1, SQLITE_TRANSIENT);
                            int colCount = sqlite3_column_count(stmt);
                            while (sqlite3_step(stmt) == SQLITE_ROW)
                            {
                                std::map<std::string, std::string> row;
                                for (int c = 0; c < colCount; ++c)
                                {
                                    const char* colName = sqlite3_column_name(stmt, c);
                                    const unsigned char* colVal = sqlite3_column_text(stmt, c);
                                    row[colName] = colVal ? reinterpret_cast<const char*>(colVal) : "";
                                }
                                dayRows.push_back(row);
                            }
                            sqlite3_finalize(stmt);
                        }
                    }
                }
                else
                {
                    std::string fullPath = path + extractMMYY_from_DDMMYY(fileNames[i]) + "/" + fileNames[i] + ".db";
                    if (!std::filesystem::exists(fullPath))
                    {
                        std::cerr << "Historical file not found: " << fullPath << std::endl;
                        Logger::log("[HPR] Historical file not found: " + fullPath);
                        return;
                    }

                    sqlite::database dayDb(fullPath);
                    sqlite3* rawDb = dayDb.connection().get();
                    sqlite3_stmt* stmt = nullptr;
                    if (sqlite3_prepare_v2(rawDb, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
                    {
                        for (size_t p = 0; p < params.size(); ++p)
                            sqlite3_bind_text(stmt, p + 1, params[p].c_str(), -1, SQLITE_TRANSIENT);
                        int colCount = sqlite3_column_count(stmt);
                        while (sqlite3_step(stmt) == SQLITE_ROW)
                        {
                            std::map<std::string, std::string> row;
                            for (int c = 0; c < colCount; ++c)
                            {
                                const char* colName = sqlite3_column_name(stmt, c);
                                const unsigned char* colVal = sqlite3_column_text(stmt, c);
                                row[colName] = colVal ? reinterpret_cast<const char*>(colVal) : "";
                            }
                            dayRows.push_back(row);
                        }
                        sqlite3_finalize(stmt);
                    }
                    else
                    {
                        std::cerr << "[DB QUERY PREPARE ERROR] " << sqlite3_errmsg(rawDb) << " | SQL: " << sql << std::endl;
                        Logger::log("[DB QUERY PREPARE ERROR] " + std::string(sqlite3_errmsg(rawDb)) + " | SQL: " + sql);
                    }
                }

                std::lock_guard<std::mutex> lock(localMutex);
                rawRows.insert(rawRows.end(), dayRows.begin(), dayRows.end());
            }
            catch (const std::exception& e)
            {
                std::cerr << "[dbQueryNumber worker] " << e.what() << std::endl;
                Logger::log("[dbQueryNumber worker] " + std::string(e.what()));
            }
        });
    }

    for (auto& w : workers) if (w.joinable()) w.join();

    out.rows = mergeQueryResults(rawRows, mode);
    out.ok = true;
    return out;
}

ParallelQueryResult DatabaseManager::dbQueryRange(std::string dateFrom, std::string dateTo, const std::string& mode, const std::string& sql, const std::vector<std::string>& params)
{
    ParallelQueryResult out;

    uint64_t timeStampFrom = parseDate_DDMMYY(dateFrom);
    uint64_t timeStampTo = parseDate_DDMMYY(dateTo);

    if (timeStampFrom == 0 || timeStampTo == 0)
    {
        std::cerr << "Invalid date format! Dates should be in DDMMYY format." << std::endl;
        Logger::log("[HPR] Invalid date format! Dates should be in DDMMYY format.");
        return out; // out.ok stays false
    }

    // Auto-swap if dateFrom is chronologically after dateTo
    if (timeStampFrom > timeStampTo)
    {
        std::swap(timeStampFrom, timeStampTo);
    }

    int days = static_cast<int>((timeStampTo - timeStampFrom) / 86400000) + 1; // +1 to include both endpoints

    std::string activeFileName = this->fileName;
    std::mutex localMutex; // local to this call only — never touches AppState
    std::vector<std::map<std::string, std::string>> rawRows;

    std::string path;
    #ifdef _WIN32
        path = std::getenv("APPDATA");
        path += "/HPR/HPR_DB/";
    #else
        const char* home = std::getenv("HOME");
        if (!home) throw std::runtime_error("HOME env var not set");
        path = home;
        path += "/.local/share/HPR/HPR_DB/";
    #endif

    // Walk backward from timeStampTo, same seed convention as loadDb_Range
    std::vector<std::string> fileNames;
    fileNames.reserve(days);
    uint64_t walkTs = timeStampTo;
    for (int i = 0; i < days; ++i)
    {
        fileNames.push_back(convertToDate_DDMMYY(walkTs));
        walkTs = (walkTs >= 86400000) ? (walkTs - 86400000) : 0;
    }

    std::vector<std::thread> workers;
    workers.reserve(days);

    for (int i = 0; i < days; ++i)
    {
        workers.emplace_back([this, i, &fileNames, &path, activeFileName, &sql, &params, &localMutex, &rawRows]()
        {
            try
            {
                std::vector<std::map<std::string, std::string>> dayRows;

                if (fileNames[i] + ".db" == activeFileName)
                {
                    // Today ,query the live connection under dbQueryMutex
                    std::lock_guard<std::mutex> lock(dbQueryMutex);
                    if (db)
                    {
                        sqlite3* rawDb = db->connection().get();
                        sqlite3_stmt* stmt = nullptr;
                        if (sqlite3_prepare_v2(rawDb, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
                        {
                            for (size_t p = 0; p < params.size(); ++p)
                                sqlite3_bind_text(stmt, p + 1, params[p].c_str(), -1, SQLITE_TRANSIENT);

                            int colCount = sqlite3_column_count(stmt);
                            while (sqlite3_step(stmt) == SQLITE_ROW)
                            {
                                std::map<std::string, std::string> row;
                                for (int c = 0; c < colCount; ++c)
                                {
                                    const char* colName = sqlite3_column_name(stmt, c);
                                    const unsigned char* colVal = sqlite3_column_text(stmt, c);
                                    row[colName] = colVal ? reinterpret_cast<const char*>(colVal) : "";
                                }
                                dayRows.push_back(row);
                            }
                            sqlite3_finalize(stmt);
                        }
                        else
                        {
                            std::cerr << "[DB QUERY PREPARE ERROR] " << sqlite3_errmsg(rawDb) << " | SQL: " << sql << std::endl;
                            Logger::log("[DB QUERY PREPARE ERROR] " + std::string(sqlite3_errmsg(rawDb)) + " | SQL: " + sql);
                        }
                    }
                }
                else
                {
                    // Past day — open the on-disk daily db file directly
                    std::string fullPath = path + extractMMYY_from_DDMMYY(fileNames[i]) + "/" + fileNames[i] + ".db";

                    if (!std::filesystem::exists(fullPath))
                    {
                        std::cerr << "Historical file not found: " << fullPath << std::endl;
                        Logger::log("[HPR] Historical file not found: " + fullPath);
                        return;
                    }

                    sqlite::database dayDb(fullPath);
                    sqlite3* rawDb = dayDb.connection().get();
                    sqlite3_stmt* stmt = nullptr;
                    if (sqlite3_prepare_v2(rawDb, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
                    {
                        for (size_t p = 0; p < params.size(); ++p)
                            sqlite3_bind_text(stmt, p + 1, params[p].c_str(), -1, SQLITE_TRANSIENT);

                        int colCount = sqlite3_column_count(stmt);
                        while (sqlite3_step(stmt) == SQLITE_ROW)
                        {
                            std::map<std::string, std::string> row;
                            for (int c = 0; c < colCount; ++c)
                            {
                                const char* colName = sqlite3_column_name(stmt, c);
                                const unsigned char* colVal = sqlite3_column_text(stmt, c);
                                row[colName] = colVal ? reinterpret_cast<const char*>(colVal) : "";
                            }
                            dayRows.push_back(row);
                        }
                        sqlite3_finalize(stmt);
                    }
                    else
                    {
                        std::cerr << "[DB QUERY PATH ERROR] " << sqlite3_errmsg(rawDb) << " | SQL: " << sql << std::endl;
                        Logger::log("[DB QUERY PATH ERROR] " + std::string(sqlite3_errmsg(rawDb)) + " | SQL: " + sql);
                    }
                }

                std::lock_guard<std::mutex> lock(localMutex);
                rawRows.insert(rawRows.end(), dayRows.begin(), dayRows.end());
            }
            catch (const std::exception& e)
            {
                std::cerr << "[dbQueryRange worker] " << e.what() << std::endl;
                Logger::log("[dbQueryRange worker] " + std::string(e.what()));
            }
        });
    }

    for (auto& w : workers)
    {
        if (w.joinable())
            w.join();
    }

    out.rows = mergeQueryResults(rawRows, mode);
    out.ok = true;
    return out;
}

std::vector<std::map<std::string, std::string>> DatabaseManager::mergeQueryResults(
    const std::vector<std::map<std::string, std::string>>& rawRows, const std::string& mode)
{
    if (mode == "raw" || rawRows.empty())
        return rawRows;

    // group by every non-numeric column; sum/average every numeric column
    std::map<std::vector<std::pair<std::string,std::string>>, std::map<std::string, double>> sums;
    std::map<std::vector<std::pair<std::string,std::string>>, std::map<std::string, int>> counts;
    std::map<std::vector<std::pair<std::string,std::string>>, std::map<std::string, std::string>> keyDisplay;

    for (const auto& row : rawRows)
    {
        std::vector<std::pair<std::string,std::string>> key;
        std::map<std::string, double> numericFields;
        for (const auto& [col, val] : row)
        {
            char* end = nullptr;
            double d = std::strtod(val.c_str(), &end);
            bool isNumeric = (end != val.c_str() && *end == '\0');
            if (isNumeric) numericFields[col] = d;
            else key.push_back({col, val});
        }
        for (const auto& [col, d] : numericFields)
        {
            sums[key][col] += d;
            counts[key][col] += 1;
        }
        keyDisplay[key] = row; // keep last row's string form for non-numeric columns
    }

    std::vector<std::map<std::string, std::string>> merged;
    for (const auto& [key, fields] : sums)
    {
        std::map<std::string, std::string> outRow = keyDisplay[key];
        for (const auto& [col, total] : fields)
        {
           double v = (mode == "average") ? (total / counts[key][col]) : total;
            if (v == std::floor(v))
            {
                outRow[col] = std::to_string(static_cast<int64_t>(v));
            }
            else
            {
                outRow[col] = std::to_string(v);
            }
        }
        merged.push_back(outRow);
    }
    return merged;
}

// Runs SELECT and returns dynamic columns and rows to Lua
std::vector<std::map<std::string, std::string>> DatabaseManager::querySQL(const std::string& sql, const std::vector<std::string>& params)
{
    if (AppState::extManager)
    {
        std::vector<CppValue> cppParams;
        for (const auto& p : params)
        {
            cppParams.push_back(CppValue(CppValue::Type::String, p));
        }
        CppValue paramsVal(CppValue::Type::Array);
        paramsVal.array_val = cppParams;

        auto res = AppState::extManager->dispatchOverride("dbQuery", { CppValue(CppValue::Type::String, sql), paramsVal });
        if (res.has_value())
        {
            std::vector<std::map<std::string, std::string>> results;
            if (res->type == CppValue::Type::Array)
            {
                for (const auto& rowVal : res->array_val)
                {
                    if (rowVal.type == CppValue::Type::Struct)
                    {
                        std::map<std::string, std::string> row;
                        for (const auto& [k, v] : rowVal.struct_val)
                        {
                            if (v.type == CppValue::Type::String)
                            {
                                row[k] = v.str_val;
                            }
                            else if (v.type == CppValue::Type::Double)
                            {
                                row[k] = std::to_string(v.double_val);
                            }
                            else if (v.type == CppValue::Type::Bool)
                            {
                                row[k] = v.bool_val ? "true" : "false";
                            }
                        }
                        results.push_back(row);
                    }
                }
            }
            return results;
        }
    }
    std::lock_guard<std::mutex> lock(dbQueryMutex);
    std::vector<std::map<std::string, std::string>> results;
    if (!db) return results;
    // Get the raw sqlite3 connection handle from sqlite_modern_cpp
    sqlite3* rawDb = db->connection().get();
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(rawDb, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "[DB QUERY PREPARE ERROR] " << sqlite3_errmsg(rawDb) << " | SQL: " << sql << std::endl;
        Logger::log("[DB QUERY PREPARE ERROR] " + std::string(sqlite3_errmsg(rawDb)) + " | SQL: " + sql);
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
    if (AppState::extManager)
    {
        std::vector<CppValue> cppParams;
        for (const auto& p : params)
        {
            cppParams.push_back(CppValue(CppValue::Type::String, p));
        }
        CppValue paramsVal(CppValue::Type::Array);
        paramsVal.array_val = cppParams;

        auto res = AppState::extManager->dispatchOverride("dbQueryPath", { CppValue(CppValue::Type::String, dbPath), CppValue(CppValue::Type::String, sql), paramsVal });
        if (res.has_value())
        {
            std::vector<std::map<std::string, std::string>> results;
            if (res->type == CppValue::Type::Array)
            {
                for (const auto& rowVal : res->array_val)
                {
                    if (rowVal.type == CppValue::Type::Struct)
                    {
                        std::map<std::string, std::string> row;
                        for (const auto& [k, v] : rowVal.struct_val)
                        {
                            if (v.type == CppValue::Type::String)
                            {
                                row[k] = v.str_val;
                            }
                            else if (v.type == CppValue::Type::Double)
                            {
                                row[k] = std::to_string(v.double_val);
                            }
                            else if (v.type == CppValue::Type::Bool)
                            {
                                row[k] = v.bool_val ? "true" : "false";
                            }
                        }
                        results.push_back(row);
                    }
                }
            }
            return results;
        }
    }
    std::vector<std::map<std::string, std::string>> results;
    try
    {
        sqlite::database targetDb(dbPath);
        sqlite3* rawDb = targetDb.connection().get();
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(rawDb, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::cerr << "[DB QUERY PATH ERROR] " << sqlite3_errmsg(rawDb) << " | SQL: " << sql << std::endl;
            Logger::log("[DB QUERY PATH ERROR] " + std::string(sqlite3_errmsg(rawDb)) + " | SQL: " + sql);
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
        Logger::log("[DB QUERY PATH EXCEPTION] " + std::string(e.what()) + " | SQL: " + sql);
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
}