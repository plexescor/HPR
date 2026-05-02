#include "HPR.hpp"
#include "appState.hpp"
#include "getCurrentWindow.hpp"

//Slint stuff
#include "app-window.h"
#include <slint.h>

#include <thread>
#include <atomic>

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
    while (running) {
        const std::string &window = getCurrentWindow();//No need to copy

        // get weak so shit doent sink (crahs)
        slint::ComponentWeakHandle<MainWindow> weak(*ui);
        slint::invoke_from_event_loop([weak, window]() {
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