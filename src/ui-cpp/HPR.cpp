#include "HPR.hpp"
#include "appState.hpp"
#include "getCurrentWindow.hpp"

//Slint stuff
#include "app-window.h"
#include <slint.h>

#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

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

    //Stuff converted for slint
    std::vector<TimeLog> vec;

    while (running) {

        {
            std::lock_guard<std::mutex> lock(AppState::stateMutex);
            window = AppState::state.currentWindow;
            vec.clear(); //Clear to avoid duplicates
            vec.reserve((*timeLog).size());
            for (const auto &[k, v] : *timeLog)
            {
                //So new maps are added at the end
                vec.insert(vec.begin(),TimeLog{slint::SharedString(k), (int)v});
            }
        }
        
        // get weak so shit doent sink (crahs)
        slint::ComponentWeakHandle<MainWindow> weak(*ui);
        slint::invoke_from_event_loop([weak, window, vec]() {
            
            if (auto handle = weak.lock()) {
                (*handle)->set_windowName(slint::SharedString(window));
                
                // 👇️ slint requires this make_shared bs
                (*handle)->set_timePerApp(std::make_shared<slint::VectorModel<TimeLog>>(vec));
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