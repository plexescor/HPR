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
    std::map<std::string, long>* timeLog = &(AppState::state.timeLog_PerApp);
    std::map<std::pair<std::string, std::string>, std::vector<uint64_t>>* switchHistory = &(AppState::state.switchHistory);
    
    //Stuff converted for slint
    std::vector<TimeLog> timeLog_Vec;
    std::vector<SwitchHistory> switchHistory_Vec;

    while (running) {

        {
            std::lock_guard<std::mutex> lock(AppState::stateMutex);
            window = AppState::state.currentWindow;

            //--------------Timelog-------------------------------------------------
            timeLog_Vec.clear(); //Clear to avoid duplicates
            timeLog_Vec.reserve((*timeLog).size());
            for (const auto &[k, v] : *timeLog)
            {
                //So new maps are added at the end
                timeLog_Vec.push_back(TimeLog{
                    slint::SharedString(k), 
                    slint::SharedString(formatTime_HHMMSS(v))
                });
            }

            //--------------SwitchHistory--------------------------------------------
            switchHistory_Vec.clear();
            switchHistory_Vec.reserve((*switchHistory).size());

            for (const auto &[k, v] : *switchHistory)
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

        }
        
        // get weak so shit doent sink (crahs)
        slint::ComponentWeakHandle<MainWindow> weak(*ui);
        slint::invoke_from_event_loop([weak, window, timeLog_Vec, switchHistory_Vec]() {
            
            if (auto handle = weak.lock()) {
                (*handle)->set_windowName_S(slint::SharedString(window));
                
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