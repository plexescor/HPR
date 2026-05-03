#include "databaseManager.hpp"
#include "timeUtils.hpp"

#include <sqlite_modern_cpp.h>

#include <chrono>
#include <iostream>
#include <string>
#include <atomic>
#include <thread>
#include <filesystem>

DatabaseManager::DatabaseManager()
{
    updateFilePath();
    updateFileName();

    sqlite::database db(filePath + fileName);
    db <<
         "create table if not exists user ("
         "   name text,"
         "   duration int"
         ");";

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

void DatabaseManager::writeLoop()
{
    //lol
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