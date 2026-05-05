#include "HPR.hpp"
#include "appState.hpp"
#include "getCurrentWindow.hpp"
#include "timeUtils.hpp"
#include "aliasManager.hpp"

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

HPR::HPR() : ui(MainWindow::create())
{
#ifdef __linux__
    slint::set_xdg_app_id("HPR"); // So it has a class in hyprland
#endif
    // ui = MainWindow::create();
}

HPR::~HPR()
{
    running = false;
    if (tracker.joinable())
        tracker.join(); // Speaks for itself
}

void HPR::show()
{
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
    // Stuff native to c++, holds raw values
    long totalTrackedTime; // For the bars
    std::string window;
    std::map<std::string, long> timeLog;
    std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;

    // Middlemen for storing raw names to converted pretty names
    std::map<std::string, long> translatedTimeLog;
    std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> translatedSwitchHistory;

    // Stuff converted for slint
    std::vector<TimeLog> timeLog_Vec;
    std::vector<SwitchHistory> switchHistory_Vec;

    while (running)
    {
        {
            totalTrackedTime = 0; // reset to 0 at every iteration

            // Clear the middlemen
            translatedTimeLog.clear();
            translatedSwitchHistory.clear();

            // Scoped mutex to hold it for as little time as possible
            {
                std::lock_guard<std::mutex> lock(AppState::stateMutex);

                // Get alias in place
                window = aliasManager.getAlias(AppState::state.currentWindow);

                timeLog = AppState::state.timeLog_PerApp;
                switchHistory = AppState::state.switchHistory;
            }

            for (const auto &[k, v] : timeLog)
            {
                totalTrackedTime += v;
                translatedTimeLog[aliasManager.getAlias(k)] += v;
            }

            timeLog_Vec.clear();
            timeLog_Vec.reserve(translatedTimeLog.size());

            for (const auto &[alias, duration] : translatedTimeLog)
            {
                timeLog_Vec.push_back(TimeLog{
                    slint::SharedString(alias),
                    slint::SharedString(formatTime_HHMMSS(duration)),
                    static_cast<int>(duration)});
            }

            // Sort so most used comes at top
            std::sort(timeLog_Vec.begin(), timeLog_Vec.end(), [](const TimeLog &a, const TimeLog &b)
                      { return a.duration_i > b.duration_i; });

            for (const auto &[k, v] : switchHistory)
            {
                const auto &[from, to] = k;
                auto &targetVec = translatedSwitchHistory[{aliasManager.getAlias(from), aliasManager.getAlias(to)}];
                targetVec.insert(targetVec.end(), v.begin(), v.end());
            }

            switchHistory_Vec.clear();
            switchHistory_Vec.reserve(translatedSwitchHistory.size());

            for (const auto &[k, v] : translatedSwitchHistory)
            {
                const auto &[from, to] = k;
                const auto &maxVal = *std::max_element(v.begin(), v.end());

                switchHistory_Vec.push_back(
                    SwitchHistory{
                        slint::SharedString(from),
                        slint::SharedString(to),
                        slint::SharedString(convertToTime_HHMMSS_12(maxVal))});
            }

            // sort so latest one comes at top
            std::sort(switchHistory_Vec.begin(), switchHistory_Vec.end(), [&translatedSwitchHistory](const SwitchHistory &a, const SwitchHistory &b)
                      {
                auto keyA = std::make_pair(std::string(a.fromWindow), std::string(a.toWindow));
                auto keyB = std::make_pair(std::string(b.fromWindow), std::string(b.toWindow));
                return *std::max_element(translatedSwitchHistory.at(keyA).begin(), translatedSwitchHistory.at(keyA).end())
                    > *std::max_element(translatedSwitchHistory.at(keyB).begin(), translatedSwitchHistory.at(keyB).end()); });
        }

        // get weak so shit doent sink (crahs)
        slint::ComponentWeakHandle<MainWindow> weak(ui);
        slint::invoke_from_event_loop([weak, window, totalTrackedTime, timeLog_Vec, switchHistory_Vec]()
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

            if (auto handle = weak.lock()) {
                (*handle)->set_windowName_S(slint::SharedString(window));

                (*handle)->set_trackedTime_S(totalTrackedTime);
                
                // 👇️ slint requires this make_shared bs
                (*handle)->set_timePerApp_S(std::make_shared<slint::VectorModel<TimeLog>>(timeLog_Vec));
            
                (*handle)->set_switchHistory_S(std::make_shared<slint::VectorModel<SwitchHistory>>(switchHistory_Vec));
            } });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void HPR::run()
{
    tracker = std::thread(&HPR::trackingLoop, this);

    ui->window().on_close_requested([this]()
    {

        #ifdef _WIN32
                // Windows - hide to tray
                ui->hide();
                return slint::CloseRequestResponse::KeepWindowShown;
        #else
                // Linux Close the app
                return slint::CloseRequestResponse::CloseWindow;
        #endif

    });

    ui->show();
    slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
    running = false;
}