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
#include <vector>
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
        
        void executeSQL(const std::string& sql, const std::vector<std::string>& params = {});
        std::vector<std::map<std::string, std::string>> querySQL(const std::string& sql, const std::vector<std::string>& params = {});
        std::vector<std::map<std::string, std::string>> querySQL_Path(const std::string& dbPath, const std::string& sql, const std::vector<std::string>& params = {});

        static std::string getDbPathForDate(const std::string& date);
        std::string getLoadedHistDbPath() const;

    private:
        void writeLoop();
        void updateFilePath();
        void updateFileName();

        void loadDb_Singular(std::string date);
        void loadDb_Number(int days, std::string mode);
        void loadDb_Range(std::string dateFrom, std::string dateTo, std::string mode);

    private:
        std::optional<sqlite::database> db;

        std::string filePath;
        std::string fileName;
        std::string loadedHistDbPath;

        std::mutex stateMutex;
        std::mutex dbQueryMutex; // Thread-safety for Lua threads

        std::atomic<bool> running{true};
        std::thread writer;

        std::map<std::string, uint64_t> timeLog_PerApp_D; 
        std::map<std::string, uint64_t> timeLog_PerTab_D; 
        std::map<std::string, uint64_t> timeLog_PerProject_D;
        std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory_D;
        std::map<std::string, int> appLimits_D;
        std::map<std::string, int> appGoals_D;
        std::map<std::string, uint64_t> limitTimeBase_D;
        std::map<std::string, uint64_t> goalTimeBase_D;

        //Ids for event listener
        size_t singular_DbLoadEventId;
        size_t number_DbLoadEventId;
        size_t range_DbLoadEventId;

        //Lock
        #ifdef _WIN32
            HANDLE lockHandle = INVALID_HANDLE_VALUE;
        #else
            int lockFd = -1;
        #endif

        struct PendingQuery {
            std::string sql;
            std::vector<std::string> params;
        };
        std::vector<PendingQuery> pendingQueries;
        std::mutex pendingQueriesMutex;

        //Async tasks
        std::future<void> historyLoadTask_Singular;
        std::future<void> historyLoadTask_Number;
        std::future<void> historyLoadTask_Range;
};