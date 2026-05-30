#include "timeUtils.hpp"

#include <cstdint>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <stdexcept>
#include <sstream>

std::string convertToDate_DDMMYY(uint64_t ms)
{
    // convert ms to sec
    std::time_t tt = static_cast<std::time_t>(ms / 1000);

    // convert to local time
    std::tm tm = *std::localtime(&tt);

    // format DD-MM-YY
    std::ostringstream oss;
    oss << std::put_time(&tm, "%d-%m-%y");

    return oss.str();
}

std::string convertToDate_MMYY(uint64_t ms)
{
     // convert ms to sec
    std::time_t tt = static_cast<std::time_t>(ms / 1000);

    // convert to local time
    std::tm tm = *std::localtime(&tt);

    // format MM/YY
    std::ostringstream oss;
    oss << std::put_time(&tm, "%m-%y");

    return oss.str();
}

std::string convertToTime_HHMMSS_12(uint64_t ms)
{
    // convert ms to seconds
    std::time_t tt = static_cast<std::time_t>(ms / 1000);

    // convert to local time
    std::tm tm = *std::localtime(&tt);

    // format HH:MM:SS AM/PM
    std::ostringstream oss;
    oss << std::put_time(&tm, "%I:%M:%S %p");

    std::string result = oss.str();

    // lower am/pm
    for (auto &c : result)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    return result; //This dealing with time in strings and uint64_ts is driving me insane
                   //Cz slint only support 32bit ints
}

std::string formatTime_HHMMSS(int ms)
{
    int totalSeconds = ms / 1000;

    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    std::ostringstream oss;

    if (hours > 0)
        oss << hours << "h ";

    if (minutes > 0 || hours > 0)
        oss << minutes << "m ";

    oss << seconds << "s";

    return oss.str();
}

uint64_t parseDate_DDMMYY(const std::string& dateStr)
{
    std::tm tm = {};
    std::istringstream iss(dateStr);
    iss >> std::get_time(&tm, "%d-%m-%y");

    if (iss.fail())
        throw std::invalid_argument("parseDate_DDMMYY: invalid format, expected DD-MM-YY, got: " + dateStr);

    tm.tm_hour = 0; tm.tm_min = 0; tm.tm_sec = 0;
    tm.tm_isdst = -1;

    std::time_t tt = std::mktime(&tm);
    if (tt == -1)
        throw std::runtime_error("parseDate_DDMMYY: mktime failed for: " + dateStr);

    return static_cast<uint64_t>(tt) * 1000ULL;
}

uint64_t parseDate_MMYY(const std::string& dateStr)
{
    std::tm tm = {};
    std::istringstream iss(dateStr);
    iss >> std::get_time(&tm, "%m-%y");

    if (iss.fail())
        throw std::invalid_argument("parseDate_MMYY: invalid format, expected MM-YY, got: " + dateStr);

    tm.tm_mday = 1; // default to 1st of the month
    tm.tm_hour = 0; tm.tm_min = 0; tm.tm_sec = 0;
    tm.tm_isdst = -1;

    std::time_t tt = std::mktime(&tm);
    if (tt == -1)
        throw std::runtime_error("parseDate_MMYY: mktime failed for: " + dateStr);

    return static_cast<uint64_t>(tt) * 1000ULL;
}

// validates DD-MM-YY format strictly, throws if wrong, returns MM-YY on success
std::string extractMMYY_from_DDMMYY(const std::string& dateStr)
{
    // must be exactly 8 chars: DD-MM-YY
    if (dateStr.size() != 8
        || !std::isdigit((unsigned char)dateStr[0])
        || !std::isdigit((unsigned char)dateStr[1])
        || dateStr[2] != '-'
        || !std::isdigit((unsigned char)dateStr[3])
        || !std::isdigit((unsigned char)dateStr[4])
        || dateStr[5] != '-'
        || !std::isdigit((unsigned char)dateStr[6])
        || !std::isdigit((unsigned char)dateStr[7]))
    {
        throw std::invalid_argument("extractMMYY_from_DDMMYY: expected DD-MM-YY, got: " + dateStr);
    }

    // Slice out day, month, and year for strict verification
    int day = std::stoi(dateStr.substr(0, 2));
    int month = std::stoi(dateStr.substr(3, 2));
    int year = std::stoi(dateStr.substr(6, 2));

    if (month < 1 || month > 12)
    {
        throw std::invalid_argument("extractMMYY_from_DDMMYY: invalid month: " + dateStr);
    }

    int daysInMonth[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int fullYear = 2000 + year;
    bool isLeap = (fullYear % 4 == 0 && (fullYear % 100 != 0 || fullYear % 400 == 0));
    if (isLeap)
    {
        daysInMonth[2] = 29;
    }

    if (day < 1 || day > daysInMonth[month])
    {
        throw std::invalid_argument("extractMMYY_from_DDMMYY: invalid day for month: " + dateStr);
    }

    // also validate via get_time so garbage like 99-99-25 gets caught
    std::tm tm = {};
    std::istringstream iss(dateStr);
    iss >> std::get_time(&tm, "%d-%m-%y");

    if (iss.fail())
        throw std::invalid_argument("extractMMYY_from_DDMMYY: date values out of range: " + dateStr);

    // slice out MM-YY (chars 3–7)
    return dateStr.substr(3); // "MM-YY"
}