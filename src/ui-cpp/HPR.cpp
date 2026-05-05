#include "HPR.hpp"
#include "appState.hpp"
#include "getCurrentWindow.hpp"
#include "timeUtils.hpp"

//Slint stuff
#include "app-window.h"
#include <slint.h>

#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>

HPR::HPR()
{
    #ifdef __linux__
        slint::set_xdg_app_id("HPR"); //So it has a class in hyprland
    #endif
    ui.emplace(MainWindow::create());

}

HPR::~HPR()
{
    running = false;
    if (tracker.joinable()) tracker.join(); //Speaks for itself
}

void HPR::trackingLoop() {
    //Stuff native to c++
    std::string window;
    std::map<std::string, long> timeLog;
    std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;
    
    //Stuff converted for slint
    std::vector<TimeLog> timeLog_Vec;
    std::vector<SwitchHistory> switchHistory_Vec;
    long totalTrackedTime;
    while (running) {
        {
            totalTrackedTime = 0;
            {
                std::lock_guard<std::mutex> lock(AppState::stateMutex);
                window = AppState::state.currentWindow;
                timeLog = AppState::state.timeLog_PerApp;
                switchHistory = AppState::state.switchHistory;
            }
            
            //--------------Timelog-------------------------------------------------
            timeLog_Vec.clear(); //Clear to avoid duplicates
            timeLog_Vec.reserve((timeLog).size());
            
            for (const auto &[k, v] : timeLog)
            {
                totalTrackedTime += v;


                //So new maps are added at the end
                timeLog_Vec.push_back(TimeLog{
                    slint::SharedString(k), 
                    slint::SharedString(formatTime_HHMMSS(v)),
                    static_cast<int>(v)
                });
            }
            std::sort(timeLog_Vec.begin(), timeLog_Vec.end(), [&timeLog](const TimeLog& a, const TimeLog& b) {
                return (timeLog).at(std::string(a.name)) > (timeLog).at(std::string(b.name));
            });

            //--------------SwitchHistory--------------------------------------------
            switchHistory_Vec.clear();
            switchHistory_Vec.reserve((switchHistory).size());

            for (const auto &[k, v] : switchHistory)
            {
                //k = pair<>, v = vector<>
                const auto& [from, to] = k;

                const auto& maxVal = *std::max_element(v.begin(), v.end());
                
                switchHistory_Vec.push_back(
                    SwitchHistory{
                        slint::SharedString(from),
                        slint::SharedString(to),
                        slint::SharedString(convertToTime_HHMMSS_12(maxVal))
                    }
                );
            }
            std::sort(switchHistory_Vec.begin(), switchHistory_Vec.end(), [&switchHistory](const SwitchHistory& a, const SwitchHistory& b) {
                auto keyA = std::make_pair(std::string(a.fromWindow), std::string(a.toWindow));
                auto keyB = std::make_pair(std::string(b.fromWindow), std::string(b.toWindow));
                return *std::max_element((switchHistory).at(keyA).begin(), (switchHistory).at(keyA).end())
                    > *std::max_element((switchHistory).at(keyB).begin(), (switchHistory).at(keyB).end());
            });

        }
        
        // get weak so shit doent sink (crahs)
        slint::ComponentWeakHandle<MainWindow> weak(*ui);
        slint::invoke_from_event_loop([weak, window, totalTrackedTime ,timeLog_Vec, switchHistory_Vec]() {
            
            if (auto handle = weak.lock()) {
                (*handle)->set_windowName_S(slint::SharedString(window));

                (*handle)->set_trackedTime_S(totalTrackedTime);
                
                // 👇️ slint requires this make_shared bs
                (*handle)->set_timePerApp_S(std::make_shared<slint::VectorModel<TimeLog>>(timeLog_Vec));
            
                (*handle)->set_switchHistory_S(std::make_shared<slint::VectorModel<SwitchHistory>>(switchHistory_Vec));
            }
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void HPR::run() {
    tracker = std::thread(&HPR::trackingLoop, this);
    //Holy dereference first
    (*ui)->run(); // guys it blocks here, well i am the sole developer
    running = false;
}