#include "databaseManager.hpp"
#include "timeUtils.hpp"
#include "appState.hpp"

#include <sqlite_modern_cpp.h>

#include <chrono>
#include <iostream>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <filesystem>

DatabaseManager::DatabaseManager()
{

    initDatabase();

    if (!loadStateFromDB())
    {
        std::cerr << "Failed to load data from db!\n";
    }
}

DatabaseManager::~DatabaseManager()
{
    running = false;
    if (writer.joinable()) writer.join();
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

        // Load app_usage into AppState
        *db << "select name, duration from app_usage;"
        >> [](std::string name, long duration) {
            AppState::state.timeLog_PerApp[name] += duration;
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
        *db << "BEGIN;";

        {
            //Copy fresh data
            std::lock_guard<std::mutex> lock(AppState::stateMutex);
            timeLog_PerApp_D = AppState::state.timeLog_PerApp;
            switchHistory_D = AppState::state.switchHistory;
        }

        for (const auto &[k, v] : timeLog_PerApp_D)
        {
            *db << "insert or replace into app_usage (name,duration) values (?,?);"
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
                AppState::state.switchHistory.clear();
            }

            // so it doesnt copy the data of previous day or whatever
            initDatabase(false);

        }

        //Sleep in 100 chunks of 100ms each so program can exit almost instantly
        for (int i = 0; i < 100 && running; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void DatabaseManager::updateFilePath()
{
    std::string tempPath;
    #ifdef _WIN32
        tempPath = std::getenv("USERPROFILE");
    #else
        const char* home = std::getenv("HOME");
        if (!home) throw std::runtime_error("HOME env var not set");
        tempPath = home;
    #endif

    //Now construct the path acc to the date

    //Get ms sinc epoch
    auto nowSystem = std::chrono::system_clock::now();
	uint64_t t = std::chrono::duration_cast<std::chrono::milliseconds>(
        nowSystem.time_since_epoch()).count();

    tempPath += "/HPR_DB/" + convertToDate_MMYY(t) + "/";
    
    filePath = tempPath;
    
    std::cout << filePath << std::endl;
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
    std::cout << tempName << std::endl;
}