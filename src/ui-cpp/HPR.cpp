#include "HPR.hpp"
#include "appState.hpp"
#include "getCurrentWindow.hpp"

//Slint stuff
#include "app-window.h"
#include <slint.h>

#include <thread>
#include <atomic>
#include <mutex>

AppState state;

HPR::HPR()
{
    #ifdef __linux__
        slint::set_xdg_app_id("HPR"); //So it has a class in hyprland
    #endif
    ui.emplace(MainWindow::create());

    getCurrentWindow_Init();
}

HPR::~HPR()
{
    running = false;
    if (tracker.joinable()) tracker.join(); //Speaks for itself
}

void HPR::trackingLoop() {
    std::string window;
    while (running) {
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            window = state.currentWindow;
        }
        
        // get weak so shit doent sink (crahs)
        slint::ComponentWeakHandle<MainWindow> weak(*ui);
        slint::invoke_from_event_loop([weak, window]() {
            // std::cout << "UI thread got: " << window << std::endl;
            if (auto handle = weak.lock()) {
                (*handle)->set_windowName(slint::SharedString(window));
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