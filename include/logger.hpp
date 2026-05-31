#pragma once
#include <string>

class Logger
{
public:
    static void log(const std::string& message);

private:
    Logger() = delete;
    static std::string getLogFilePath();
};