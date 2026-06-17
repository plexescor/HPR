#pragma once

#include <string>

class TelemetryManager
{
public:
    static void init();
    static void checkAndSend();

private:
    static std::string generateUUID();
};
