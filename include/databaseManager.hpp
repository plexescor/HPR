#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <map>
#include <cstdint>
#include <optional>
#include <vector>
#include <sqlite_modern_cpp.h>
#include <future>

#ifdef _WIN32
    #include <windows.h>
#endif

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

        void loadDb_Singular(std::string date);

    private:
        std::optional<sqlite::database> db;

        std::string filePath;
        std::string fileName;

        std::mutex stateMutex;

        std::atomic<bool> running{true};
        std::thread writer;

        std::map<std::string, long> timeLog_PerApp_D; 
        std::map<std::string, long> timeLog_PerTab_D; 
        std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory_D;

        //Ids for event listener
        size_t singular_DbLoadEventId;

        //Lock
        #ifdef _WIN32
            HANDLE lockHandle = INVALID_HANDLE_VALUE;
        #else
            int lockFd = -1;
        #endif

        //Async tasks
        std::future<void> historyLoadTask_Singular;
};