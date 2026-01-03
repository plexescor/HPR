#pragma once
#include "diskWriteHandlerMain.hpp"

int writeWindowNameAndDurationToDisk(const char* windowName, int duration, int switchCount);
bool readWindowNameAndDurationFromDisk(std::map<std::string, double>* timeLog, int* switches);