#include "timeUtils.hpp"

#include <cstdint>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

std::string convertToDate_DDMMYY(uint64_t ms)
{
    // convert ms to sec
    std::time_t tt = static_cast<std::time_t>(ms / 1000);

    // convert to local time
    std::tm tm = *std::localtime(&tt);

    // format DD/MM/YY
    std::ostringstream oss;
    oss << std::put_time(&tm, "%d/%m/%y");

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