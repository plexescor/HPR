#include "HPR.hpp"
#include "appState.hpp"
#include "getCurrentWindow.hpp"
#include "timeUtils.hpp"
#include "aliasManager.hpp"
#include "uiEventBridge.hpp"

// Slint stuff
#include "app-window.h"
#include <slint.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>

HPR::HPR() : ui(MainWindow::create()), modelManager(ui)
{
    #ifdef __linux__
        slint::set_xdg_app_id("HPR"); // So it has a class in hyprland
    #endif
}

HPR::~HPR()
{
    running = false;
    if (tracker.joinable())
        tracker.join(); // Speaks for itself
}

void HPR::show()
{
    // This sets the title bar icon on windows
    // 🖕 windows and microslop
    #ifdef _WIN32
        HWND hwnd = FindWindowW(nullptr, L"HPR");
        if (hwnd) {
            HICON hIconBig = (HICON)LoadImage(
                GetModuleHandle(NULL), MAKEINTRESOURCE(1),
                IMAGE_ICON, 32, 32, 0  // explicit 32x32 for title bar
            );
            HICON hIconSmall = (HICON)LoadImage(
                GetModuleHandle(NULL), MAKEINTRESOURCE(1),
                IMAGE_ICON, 16, 16, 0  // explicit 16x16
            );
            if (hIconBig)   SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIconBig);
            if (hIconSmall) SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
        }
    #endif
    
    auto weak = slint::ComponentWeakHandle<MainWindow>(ui);

    slint::invoke_from_event_loop([weak]()
                                  {
        if (auto handle = weak.lock()) 
        {
            (*handle)->show();
        } });
}

void HPR::quit()
{
    slint::invoke_from_event_loop([]()
                                  { slint::quit_event_loop(); });
}

void HPR::hide()
{
    slint::invoke_from_event_loop([this]()
                                  { ui->hide(); });
}

void HPR::trackingLoop()
{

    // This sets the title bar icon on windows
    // 🖕 windows and microslop
    #ifdef _WIN32
        HWND hwnd = FindWindowW(nullptr, L"HPR");
        if (hwnd) {
            HICON hIconBig = (HICON)LoadImage(
                GetModuleHandle(NULL), MAKEINTRESOURCE(1),
                IMAGE_ICON, 32, 32, 0  // explicit 32x32 for title bar
            );
            HICON hIconSmall = (HICON)LoadImage(
                GetModuleHandle(NULL), MAKEINTRESOURCE(1),
                IMAGE_ICON, 16, 16, 0  // explicit 16x16
            );
            if (hIconBig)   SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIconBig);
            if (hIconSmall) SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
        }
    #endif

    // Stuff native to c++, holds raw values
    long totalTrackedTime; // For the bars
    std::string window;
    std::map<std::string, long> timeLog;
    std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;

    while (running)
    {
        {
            totalTrackedTime = 0; // reset to 0 at every iteration
            // Scoped mutex to hold it for as little time as possible
            {
                std::lock_guard<std::mutex> lock(AppState::stateMutex);
                window = AppState::state.currentWindow;
                timeLog = AppState::state.timeLog_PerApp;
                switchHistory = AppState::state.switchHistory;
            }

            modelManager.update(
                timeLog,
                switchHistory,
                window,
                totalTrackedTime, 
                aliasManager
            );
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void HPR::run()
{
    tracker = std::thread(&HPR::trackingLoop, this);

    UiEventBridge uiEventBridge(ui);

    #ifdef _WIN32
        ui->window().on_close_requested([this]() -> slint::CloseRequestResponse {
            ui->hide();
            return slint::CloseRequestResponse::KeepWindowShown;
        });
    #else
        ui->window().on_close_requested([this]() -> slint::CloseRequestResponse {
            running = false;
            slint::quit_event_loop();
            return slint::CloseRequestResponse::KeepWindowShown;
        });
    #endif

    ui->show();
    slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
    running = false; // safety net
}