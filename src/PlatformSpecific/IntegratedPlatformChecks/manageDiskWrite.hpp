#pragma once
#include "diskWriteHandlerMain.hpp"

int writeWindowNameAndDurationToDisk(const char* windowName, int duration, int switchCount);
bool readWindowNameAndDurationFromDisk(std::map<std::string, double>* timeLog, int* switches);
int writeWindowSwitchToDisk(const char* fromWindow, const char* toWindow);
bool readWindowSwitchFromDisk(std::vector<std::pair<std::string, std::string>>* switchData);