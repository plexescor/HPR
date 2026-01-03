#include <iostream>
#include <string>
#include <chrono>
#include <map>
#include <ranges> //For max value in a vector

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
       
        int finalSwitchValue = *std::ranges::max_element(allSwitchValues); //Find the max value of vec for correct switch count
        *switches = finalSwitchValue - 1; //Assign it to dereferenced switches, DO -1 BECAUSE OPENING HPR COUNTS AS SWITCHES, I DONT WANT THAT

        freeAppUsageData(apps); //IMPORTANT TO PREVENT LEAKS
        closeHandler();
        return true;
    }
    closeHandler();
    return false;
    
}
