#include "app-window.h"

#include "HPR.hpp"
#include "appState.hpp"
#include "getCurrentWindow.hpp"
#include "timeUtils.hpp"
#include "aliasManager.hpp"
#include "uiEventBridge.hpp"
#include "appEvents.hpp"
#include "patternAnalyzer.hpp"
#include "windowUtilities.hpp"

// Slint stuff
#include <slint.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>
#include <chrono>

HPR::HPR(ExtensionManager* extMgr) : ui(MainWindow::create()), modelManager(ui)
{
    if (extMgr)
        this->extManager = extMgr;
    #ifdef __linux__
        slint::set_xdg_app_id("HPR"); // So it has a class in hyprland
    #endif
    
}

HPR::~HPR()
{
    running = false;
    if (tracker.joinable())
        tracker.join(); // Speaks for itself

    EventHub::disconnect(Event::APP_ERROR, errorId);
}

void HPR::show()
{
    auto weak = slint::ComponentWeakHandle<MainWindow>(ui);

    slint::invoke_from_event_loop([weak]()
                                  {
        if (auto handle = weak.lock()) 
        {
            (*handle)->show();
            #ifdef _WIN32
                HWND hwnd = FindWindowW(nullptr, L"HPR");
                if (hwnd) {
                    HICON hIconBig = (HICON)LoadImage(
                        GetModuleHandle(NULL), MAKEINTRESOURCE(1),
                        IMAGE_ICON, 32, 32, LR_SHARED
                    );
                    HICON hIconSmall = (HICON)LoadImage(
                        GetModuleHandle(NULL), MAKEINTRESOURCE(1),
                        IMAGE_ICON, 16, 16, LR_SHARED
                    );
                    if (hIconBig)   SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIconBig);
                    if (hIconSmall) SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
                    ShowWindow(hwnd, SW_SHOW);
                    ShowWindow(hwnd, SW_RESTORE);
                    SetForegroundWindow(hwnd);
                }
            #endif
        } });
}

void HPR::quit()
{
    slint::invoke_from_event_loop([]()
                                  { slint::quit_event_loop(); });
}

void HPR::hide()
{
    auto weak = slint::ComponentWeakHandle<MainWindow>(ui);
    slint::invoke_from_event_loop([weak]() {
        if (auto handle = weak.lock())
            (*handle)->hide();
    });
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
                IMAGE_ICON, 32, 32, LR_SHARED
            );
            HICON hIconSmall = (HICON)LoadImage(
                GetModuleHandle(NULL), MAKEINTRESOURCE(1),
                IMAGE_ICON, 16, 16, LR_SHARED
            );
            if (hIconBig)   SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIconBig);
            if (hIconSmall) SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
        }
    #endif

    errorId = EventHub::connect(Event::APP_ERROR, [this] (EventData data)
    {
        if (std::holds_alternative<ErrorGui>(data)) 
        {
            std::string error = std::get<ErrorGui>(data).error;
            activeGuiError = error;
            if (!error.empty()) 
            {
                errorTimestamp = std::chrono::steady_clock::now();
            }
        }
    });

    bool uiReady = false;

    // Stuff native to c++, holds raw values
    uint64_t totalTrackedTime;
    uint64_t totalTrackedTime_Tab;
    uint64_t totalTrackedTime_Project;
    std::string window;
    std::map<std::string, uint64_t> timeLog;
    std::map<std::string, uint64_t> timeLog_Tab;
    std::map<std::string, uint64_t> timeLog_Project;
    std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;

    //Special priority to insights because they arent exactly on demand loaded,
    //rather updated every 5 minutes
    auto lastInsightUpdate = std::chrono::steady_clock::now();
    bool firstRun = true;

    while (running)
    {
        {
            auto now = std::chrono::steady_clock::now();
            
            totalTrackedTime = 0; // reset to 0 at every iteration
            totalTrackedTime_Tab = 0;
            totalTrackedTime_Project = 0;
            // Scoped mutex to hold it for as little time as possible
            {
                std::lock_guard<std::mutex> lock(AppState::stateMutex);
                    
                //If live view, show today's data
                if (AppState::state.currentView == AppState::CurrentView::LIVE)
                {
                    window = AppState::state.currentWindow;
                    timeLog = AppState::state.timeLog_PerApp;
                    timeLog_Tab = AppState::state.timeLog_PerTab;
                    timeLog_Project = AppState::state.timeLog_PerProject;
                    switchHistory = AppState::state.switchHistory;
                }
                else if (AppState::state.currentView == AppState::CurrentView::HISTORICAL_SINGULAR)
                {
                    std::lock_guard<std::mutex> histLock(AppState::historyStateMutex);
                    window = AppState::state.currentWindow;
                    timeLog = AppState::historicalData_State.timeLog_PerApp;
                    timeLog_Tab = AppState::historicalData_State.timeLog_PerTab;
                    timeLog_Project = AppState::historicalData_State.timeLog_PerProject;
                    switchHistory = AppState::historicalData_State.switchHistory;
                }
                else if (AppState::state.currentView == AppState::CurrentView::HISTORICAL_NUMBER || AppState::state.currentView == AppState::CurrentView::HISTORICAL_RANGE)
                {
                    std::lock_guard<std::mutex> histLock(AppState::historyStateMutex);
                    window = AppState::state.currentWindow;
                    timeLog = AppState::historicalData_Full_State.timeLog_PerApp;
                    timeLog_Tab = AppState::historicalData_Full_State.timeLog_PerTab;
                    timeLog_Project = AppState::historicalData_Full_State.timeLog_PerProject;
                    switchHistory = AppState::historicalData_Full_State.switchHistory;
                }
            }

            //Overwrite the window variable if we have an active error
            if (!activeGuiError.empty())
            {
                auto now = std::chrono::steady_clock::now();
                int errorDuration = AppState::configManager.getConfig("ui-error-duration", 5000);
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - errorTimestamp).count() >= errorDuration)
                {
                    // clear the error
                    activeGuiError = "";
                }
                else
                {
                    // keep showing it
                    window = "Error: " + activeGuiError;
                }
            }

            modelManager.update(
                timeLog,
                timeLog_Tab,
                timeLog_Project,
                switchHistory,
                window,
                totalTrackedTime,
                totalTrackedTime_Tab, 
                totalTrackedTime_Project,
                AppState::aliasManager
            );

            {
                std::lock_guard<std::mutex> lock(AppState::patternAnalyzerMutex);
                AppState::patternAnalyzer.generateInsights();
            }

            // Update insight (or on first frame)
            int insightInterval = AppState::configManager.getConfig("ui-insight-interval", 1000);
            if (firstRun || std::chrono::duration_cast<std::chrono::milliseconds>(now - lastInsightUpdate).count() >= insightInterval) 
            {
                std::lock_guard<std::mutex> lock(AppState::patternAnalyzerMutex);
                
                modelManager.showInsights(
                    AppState::patternAnalyzer.getMostUsed(),
                    AppState::patternAnalyzer.getTotalTrackedTime(),
                    AppState::patternAnalyzer.getSwitchCount(),
                    AppState::patternAnalyzer.getMostSwitchedFrom(),
                    AppState::patternAnalyzer.getMostSwitchedTo(),
                    AppState::patternAnalyzer.getMostFocusedSession(),
                    AppState::patternAnalyzer.getMostProductiveHour(),
                    AppState::patternAnalyzer.getEscapePattern(),
                    AppState::patternAnalyzer.getReturnRate(),
                    AppState::patternAnalyzer.getAvgFocusSession(),
                    AppState::patternAnalyzer.getMostDistractedDay(),
                    AppState::patternAnalyzer.getProductiveDaysThisWeek(),
                    AppState::patternAnalyzer.getScreenTimeVsAverage(),
                    AppState::patternAnalyzer.getFocusDipHour(),
                    AppState::patternAnalyzer.getDeepWorkBeforeNoon(),
                    AppState::patternAnalyzer.getWeekendVsWeekday()
                );
                lastInsightUpdate = now;
                firstRun = false;
            }

            std::vector<std::pair<std::string,std::string>> extensionsCopy;
            {
                std::lock_guard<std::mutex> lock(AppState::stateMutex);
                extensionsCopy = AppState::state.loadedExtensions;
            }
            modelManager.showExtensions(extensionsCopy);
            modelManager.showFunStats(
                getCpuUsage(),
                getRamUsage(),
                std::to_string(extensionsCopy.size()),
                getThreadCount()
            );
        }
        if (!uiReady)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            uiReady = true;
            EventHub::emit(Event::UI_READY);
        }
        else
        {
            //Chunked sleep based on ui-update-interval
            int uiUpdateInterval = AppState::configManager.getConfig("ui-update-interval", 200);
            int remaining = uiUpdateInterval;
            while (remaining > 0 && running)
            {
                int sleepTime = std::min(remaining, 50);
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
                remaining -= sleepTime;
            }
        }    
    }
}

void HPR::run()
{
    UiEventBridge uiEventBridge(ui, extManager);

    #ifdef _WIN32
        ui->window().on_close_requested([this]() -> slint::CloseRequestResponse {
            ui->hide();
            return slint::CloseRequestResponse::KeepWindowShown;
        });
    #else
        // same as windows, X button just hides to tray
        ui->window().on_close_requested([this]() -> slint::CloseRequestResponse {
            ui->hide();
            return slint::CloseRequestResponse::KeepWindowShown;
        });
    #endif

    bool headless = AppState::configManager.getConfig("headless-mode", false);
    if (!headless)
    {
        std::cout << "Showing ui\n";
        ui->show();
    }
    else
    {
        ui->show();
        auto weak = slint::ComponentWeakHandle<MainWindow>(ui);
        slint::invoke_from_event_loop([weak]() {
            if (auto handle = weak.lock())
                (*handle)->hide();
        });
    }

    #ifdef _WIN32
    // post to event loop so it runs AFTER the window is actually visible
    // cz when app is launched frshly, theres no icon
    slint::invoke_from_event_loop([]() {
        HWND hwnd = FindWindowW(nullptr, L"HPR");
        if (hwnd) {
            HICON hIconBig = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1),
                IMAGE_ICON, 32, 32, LR_SHARED);
            HICON hIconSmall = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1),
                IMAGE_ICON, 16, 16, LR_SHARED);
            if (hIconBig)   SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIconBig);
            if (hIconSmall) SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
        }
    });
    #endif

    tracker = std::thread(&HPR::trackingLoop, this);
    slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
    running = false; // safety net
}