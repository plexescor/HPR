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
    updateFilePath();
    updateFileName();


    //We need copies of the data for extra safety
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        timeLog_PerApp_D = AppState::state.timeLog_PerApp;
        switchHistory_D = AppState::state.switchHistory;
    }

    db.emplace(filePath + fileName);

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

        std::cout << "Done DB ops!" << std::endl;
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
        tempPath = std::getenv("HOME");
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