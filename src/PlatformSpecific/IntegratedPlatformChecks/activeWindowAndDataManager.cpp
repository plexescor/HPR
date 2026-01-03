//I suppose this file WAS so easy to understand that you DIDNT need comments
#include <string>
#include <chrono>
#include <map>
#include <iostream>

#include "windowDetection.hpp"
#include "validateWindow.hpp"
#include "manageDiskWrite.hpp"

namespace activeWindowAndDataManagement
{
    bool coldStart = true;
    
    struct windowInfoAndData{
        std::string currentWindow;
        std::string previousWindow;
        
        int switches = -1; //PLS START AT -1, else, it increments to 1 when app is launched

        // Map to store: ["Chrome.exe", 120 seconds] //This is for visualisation
        std::map<std::string, double> timeLog;

    } wInfoAndD;

    int cooldown = 0;
    auto lastTimestamp = std::chrono::steady_clock::now(); //Get the tick's time

    //Just updates the current values while adhering to checks
    int updateCurrentWindowInfoAndData()
    {   
        //IF APP IS STARTED FOR FIRST TIME, READ EXISTING VALUE FROM DISK
        if (coldStart)
        {
            if(!readWindowNameAndDurationFromDisk(&wInfoAndD.timeLog, &wInfoAndD.switches)) 
            {
                std::cout << "Couldnt read the db, possibly because you are launching the app for the very very first time";
            }
            coldStart = false;
        }
        cooldown++; //Implementing a cooldown
        // std::cout << cooldown <<'\n';

        if (cooldown < 100) return 2; //Waiting
        
        cooldown = 0;

        // 1. Calculate elapsed time since the LAST time we were here, ~CLAUDE
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - lastTimestamp;
        lastTimestamp = now; // Reset for next time

        //Get current window
        wInfoAndD.currentWindow = getActiveProcessName();

        if (isValidWindow(wInfoAndD.currentWindow))
        {
            // 2. Add the elapsed time to the CURRENTLY active app
            // if app aint in map, it gets created automatically
            //WE NEED THIS TO BE UPDATED EVERYTIME SO THE TIME ELAPSED REMAINS ACCURATE
            wInfoAndD.timeLog[wInfoAndD.currentWindow] += elapsed.count();

            //WRITE INFO TO DISK
            writeWindowNameAndDurationToDisk(
                wInfoAndD.currentWindow.c_str(), 
                wInfoAndD.timeLog[wInfoAndD.currentWindow], 
                wInfoAndD.switches
            );


            if (wInfoAndD.currentWindow != wInfoAndD.previousWindow)
            {
                wInfoAndD.previousWindow = wInfoAndD.currentWindow;
                wInfoAndD.switches++;
                return 0; //Successfully reassigned shit, stops the execution to prevent accidental reassignment in the next line
            }


            return 0;
        }

        wInfoAndD.currentWindow = wInfoAndD.previousWindow; //This means if current window holds an "invalid window", reassign the value of previous window to it
        return 1;
    }

    int getCurrentSwitchCount()
    {
        updateCurrentWindowInfoAndData(); //Update before returning
        return wInfoAndD.switches;
    }

    std::string getCurrentWindowName()
    {
        updateCurrentWindowInfoAndData(); //Same as abbove
        return wInfoAndD.currentWindow;
    }

    std::map<std::string, double>* getTimeLog()
    {
        updateCurrentWindowInfoAndData();
        return &(wInfoAndD.timeLog);
    }
}

