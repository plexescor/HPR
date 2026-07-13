#include "timeUtils.hpp"

#include <cstdint>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <stdexcept>
#include <sstream>

std::tm safe_localtime(std::time_t tt)
{
    std::tm tm = {};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    return tm;
}

std::string convertToDate_DDMMYY(uint64_t ms)
{
    try
    {
        std::time_t tt = static_cast<std::time_t>(ms / 1000);
        std::tm tm = safe_localtime(tt);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%d-%m-%y");
        return oss.str();
    }
    catch (...) { return ""; }
}

std::string convertToDate_MMYY(uint64_t ms)
{
    try
    {
        std::time_t tt = static_cast<std::time_t>(ms / 1000);
        std::tm tm = safe_localtime(tt);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%m-%y");
        return oss.str();
    }
    catch (...) { return ""; }
}

std::string convertToTime_HHMMSS_12(uint64_t ms)
{
    try
    {
        std::time_t tt = static_cast<std::time_t>(ms / 1000);
        std::tm tm = safe_localtime(tt);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%I:%M:%S %p");
        std::string result = oss.str();
        for (auto& c : result)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return result;
    }
    catch (...) { return ""; }
}

std::string formatTime_HHMMSS(uint64_t ms)
{
    try
    {
        uint64_t totalSeconds = ms / 1000;
        uint64_t hours   = totalSeconds / 3600;
        uint64_t minutes = (totalSeconds % 3600) / 60;
        uint64_t seconds = totalSeconds % 60;

        std::ostringstream oss;
        if (hours > 0)   oss << hours   << "h ";
        if (minutes > 0 || hours > 0) oss << minutes << "m ";
        oss << seconds << "s";
        return oss.str();
    }
    catch (...) { return "0s"; }
}

uint64_t parseDate_DDMMYY(const std::string& dateStr)
{
    try
    {
        if (dateStr.empty()) return 0;

        std::tm tm = {};
        std::istringstream iss(dateStr);
        iss >> std::get_time(&tm, "%d-%m-%y");
        if (iss.fail()) return 0;

        tm.tm_hour = 0; tm.tm_min = 0; tm.tm_sec = 0;
        tm.tm_isdst = -1;

        std::time_t tt = std::mktime(&tm);
        if (tt == -1) return 0;

        return static_cast<uint64_t>(tt) * 1000ULL;
    }
    catch (...) { return 0; }
}

uint64_t parseDate_MMYY(const std::string& dateStr)
{
    try
    {
        if (dateStr.empty()) return 0;

        std::tm tm = {};
        std::istringstream iss(dateStr);
        iss >> std::get_time(&tm, "%m-%y");
        if (iss.fail()) return 0;

        tm.tm_mday = 1;
        tm.tm_hour = 0; tm.tm_min = 0; tm.tm_sec = 0;
        tm.tm_isdst = -1;

        std::time_t tt = std::mktime(&tm);
        if (tt == -1) return 0;

        return static_cast<uint64_t>(tt) * 1000ULL;
    }
    catch (...) { return 0; }
}

std::string extractMMYY_from_DDMMYY(const std::string& dateStr)
{
    try
    {
        if (dateStr.size() != 8
            || !std::isdigit((unsigned char)dateStr[0])
            || !std::isdigit((unsigned char)dateStr[1])
            || dateStr[2] != '-'
            || !std::isdigit((unsigned char)dateStr[3])
            || !std::isdigit((unsigned char)dateStr[4])
            || dateStr[5] != '-'
            || !std::isdigit((unsigned char)dateStr[6])
            || !std::isdigit((unsigned char)dateStr[7]))
            return "";

        int day   = std::stoi(dateStr.substr(0, 2));
        int month = std::stoi(dateStr.substr(3, 2));
        int year  = std::stoi(dateStr.substr(6, 2));

        if (month < 1 || month > 12) return "";

        int daysInMonth[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        int fullYear = 2000 + year;
        bool isLeap = (fullYear % 4 == 0 && (fullYear % 100 != 0 || fullYear % 400 == 0));
        if (isLeap) daysInMonth[2] = 29;

        if (day < 1 || day > daysInMonth[month]) return "";

        std::tm tm = {};
        std::istringstream iss(dateStr);
        iss >> std::get_time(&tm, "%d-%m-%y");
        if (iss.fail()) return "";

        return dateStr.substr(3);
    }
    catch (...) { return ""; }
}