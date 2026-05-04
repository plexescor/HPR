#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <map>
#include <cstdint>
#include <optional>
#include <vector>
#include <sqlite_modern_cpp.h>

class DatabaseManager
{
    public:
        DatabaseManager();
        ~DatabaseManager();
        void initDatabase(bool copyData = true);
        void run();
        bool loadStateFromDB();

    private:
        void writeLoop();
        void updateFilePath();
        void updateFileName();

    private:
        std::optional<sqlite::database> db;

        std::string filePath;
        std::string fileName;

        std::atomic<bool> running{true};
        std::thread writer;

        std::map<std::string, long> timeLog_PerApp_D; 
        std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory_D;
};