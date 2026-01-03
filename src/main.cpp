#include "mainGUI.hpp"
#include <iostream>

int main()
{
    initGUI();
    runGUI();
    quitGUI();

    // testDb();
    
    return 0;
}
//-------------------------------------------------------------------------------------------------------
//
//                                              TEST AREA
//
//-------------------------------------------------------------------------------------------------------

// void testDb()
// {
//     // Initialize
//     initHandler("test.db");
    
//     // Write app usage
//     writeAppUsage("Chrome", 120);
//     writeAppUsage("Code", 300);
//     writeAppUsage("Chrome", 60);
    
//     // Write switches
//     writeSwitch("Chrome", "Code");
//     writeSwitch("Code", "Notepad");
//     writeSwitch("Notepad", "Chrome");
    
//     // Read single app usage
//     int chromeTime = readAppUsage("Chrome");
//     std::cout << "\nChrome total time: " << chromeTime << " seconds" << std::endl;
    
//     // Read all app usage
//     int appCount = 0;
//     AppUsageData* apps = readAllAppUsage(&appCount);
//     if (apps) {
//         std::cout << "\nAll apps usage:" << std::endl;
//         for (int i = 0; i < appCount; i++) {
//             std::cout << "  " << apps[i].appName << ": " << apps[i].totalDuration << "s" << std::endl;
//         }
//         freeAppUsageData(apps);
//     }
    
//     // Read all switches
//     int switchCount = 0;
//     SwitchData* switches = readAllSwitches(&switchCount);
//     if (switches) {
//         std::cout << "\nAll switches:" << std::endl;
//         for (int i = 0; i < switchCount; i++) {
//             std::cout << "  " << switches[i].fromApp << " -> " << switches[i].toApp 
//                       << " (timestamp: " << switches[i].timestamp << ")" << std::endl;
//         }
//         freeSwitchData(switches);
//     }
    
//     // Read switches FROM Chrome
//     int fromCount = 0;
//     SwitchData* fromChrome = readSwitchesFrom("Chrome", &fromCount);
//     if (fromChrome) {
//         std::cout << "\nSwitches FROM Chrome:" << std::endl;
//         for (int i = 0; i < fromCount; i++) {
//             std::cout << "  Chrome -> " << fromChrome[i].toApp << std::endl;
//         }
//         freeSwitchData(fromChrome);
//     }
    
//     // Read switches TO Code
//     int toCount = 0;
//     SwitchData* toCode = readSwitchesTo("Code", &toCount);
//     if (toCode) {
//         std::cout << "\nSwitches TO Code:" << std::endl;
//         for (int i = 0; i < toCount; i++) {
//             std::cout << "  " << toCode[i].fromApp << " -> Code" << std::endl;
//         }
//         freeSwitchData(toCode);
//     }
    
//     // Close
//     closeHandler();
// }
