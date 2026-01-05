#include <iostream>
#include <string>
#include <chrono>
#include <map>
#include <algorithm> //For max_element, //Maybe not

#include "diskWriteHandlerMain.hpp"

int writeWindowNameAndDurationToDisk(const char* windowName, int duration, int switchCount)
{
    //Open the database, or create if needed
    if(!initHandler("test.db")) 
    {
        closeHandler();
        return 1; //cannot open/create database
    }

    if(!writeAppUsage(windowName, duration, switchCount)) 
    {
        closeHandler();
        return 2; //Error while writing app usage
    }

    closeHandler();
    return 0;
}

bool readWindowNameAndDurationFromDisk(std::map<std::string, double>* timeLog, int* switches)
{
    if(!initHandler("test.db"))
    {
        closeHandler();
        return false;
    }

    //Create  vector of all switch value obtained in next step
    std::vector<int> allSwitchValues;

    //Sorry!
    int appCount = 0;
    AppUsageData* apps = readAllAppUsage(&appCount);
    if (apps) 
    {
        for (int i = 0; i < appCount; i++) 
        {
            //Do this do that, create key named appName with value of total duration
            //btw map will always be empty because this func is designed to run at cold start

            (*timeLog)[apps[i].appName] = apps[i].totalDuration;
            allSwitchValues.push_back(apps[i].switchCount); //We will find the max value and get switch count, i know its messy
        }

        //Sanity check
        if (allSwitchValues.empty()) 
        {
            std::cout << "The vector is empty." << std::endl;
            closeHandler();
            return false; //Cannot extract switch count
        }
       
        int finalSwitchValue = *std::max_element(allSwitchValues.begin(), allSwitchValues.end()); //Find the max value of vec for correct switch count
        *switches = finalSwitchValue; //Assign it to dereferenced switches
        freeAppUsageData(apps); //IMPORTANT TO PREVENT LEAKS
        closeHandler();
        return true;
    }
    closeHandler();
    return false;
    
}

int writeWindowSwitchToDisk(const char* fromWindow, const char* toWindow)
{
    if(!initHandler("test.db")) return 1;
    writeSwitch(fromWindow, toWindow);
    closeHandler();
    return 0;
}

bool readWindowSwitchFromDisk(std::vector<std::pair<std::string, std::string>>* switchData)
{
    if(!switchData) return false;
    
    if(!initHandler("test.db"))
    {
        closeHandler();
        return false;
    }

    int switchCount = 0;
    SwitchData* switches = readAllSwitches(&switchCount);
    
    if (switches) 
    {
        for (int i = 0; i < switchCount; i++) 
        {
            switchData->emplace_back
            (
            switches[i].fromApp,
            switches[i].toApp
            );
        }
        freeSwitchData(switches);
        closeHandler();
        return true;
    }

    closeHandler();
    return false;
}