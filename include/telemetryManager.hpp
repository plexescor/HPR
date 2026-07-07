#pragma once

#include <string>

class TelemetryManager
{
public:
    static void init();
    static void checkAndSend();

private:
    static std::string generateUUID();
    static void privilegedAggregationCycle();
    static int countJsonTopLevelKeys(const std::string& json);
};
