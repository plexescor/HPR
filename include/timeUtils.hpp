#pragma once
#include <string>
#include <cstdint>

std::string convertToDate_DDMMYY(uint64_t ms);
std::string convertToDate_MMYY(uint64_t ms);
std::string convertToTime_HHMMSS_12(uint64_t ms);
std::string formatTime_HHMMSS(uint64_t ms);
uint64_t parseDate_DDMMYY(const std::string& dateStr);
uint64_t parseDate_MMYY(const std::string& dateStr);
std::string extractMMYY_from_DDMMYY(const std::string& dateStr);