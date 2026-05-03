#pragma once
#include <string>
#include <atomic>
#include <thread>

class DatabaseManager
{
    public:
        DatabaseManager();
        ~DatabaseManager();
        void run();

    private:
        void writeLoop();
        void updateFilePath();
        void updateFileName();

    private:
        std::string filePath;
        std::string fileName;

        std::atomic<bool> running{true};
        std::thread writer;
};