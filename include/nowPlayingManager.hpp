#pragma once

#include <string>

class NowPlayingManager
{
public:
    static void init();
    static void runCycle();

private:
    static std::string extractJsonValue(const std::string& json, const std::string& key);
    static std::string urlEncode(const std::string& value);
    static std::string escapeJsonString(const std::string& s);
    static std::string toLower(const std::string& s);
};
