#include "HPRInterpreter.hpp"

#include <slint-interpreter.h>

#include "appState.hpp"
#include "getCurrentWindow.hpp"
#include "timeUtils.hpp"
#include "aliasManager.hpp"
#include "uiEventBridge.hpp"
#include "appEvents.hpp"
#include "patternAnalyzer.hpp"
#include "logger.hpp"
#include "uiRegistry.hpp"
#include "extensionManager.hpp"
#include "windowUtilities.hpp"
#include <thread>
#include <atomic>
#include <optional>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <condition_variable>
#include <mutex>
#include "logger.hpp"
#include <chrono>

#ifdef _WIN32
    #include "Windows.h"
#endif

HPRInterpreter::HPRInterpreter(ExtensionManager* extMgr)
{
    if (extMgr)
        this->extManager = extMgr;

    if (!initialiseSlintUiPath()) exit(1);

    std::string savedTheme = AppState::configManager.getConfig("custom-theme", std::string("default"));
    std::string pathToLoad = filePath + fileName;
    if (savedTheme != "default" && AppState::themeManager.availableThemes_Bare.contains(savedTheme))
    {
        pathToLoad = AppState::themeManager.availableThemes_Bare[savedTheme];
    }

    compiler = std::make_unique<slint::interpreter::ComponentCompiler>();
    definition = compiler->build_from_path(pathToLoad);

    if (!definition.has_value()) 
    {
        for (auto& diag : compiler->diagnostics())
        {
            fprintf(stderr, "  → %s\n", diag.message.data());
            Logger::log(diag.message.data());
        }   
        exit(1);
    }

    instance = definition->create();
    weak_instance = instance.value();

    //give ui registery for what it demands
    UiRegistry::registerInstance(weak_instance.value());

    modelManager.emplace(instance.value());
    #ifdef __linux__
        slint::set_xdg_app_id("HPR"); // So it has a class in hyprland
    #endif
}

HPRInterpreter::~HPRInterpreter()
{
    running = false;

    // Wake up the thread if it's trapped in a hidden pause cycle during shutdown
    pauseCv.notify_all(); 

    if (tracker.joinable())
        tracker.join(); // Speaks for itself

    EventHub::disconnect(Event::APP_ERROR, errorId);
}

void HPRInterpreter::reload(std::string path)
{
    // local compiler, no need to store it — just swap definition + instance
    slint::interpreter::ComponentCompiler newCompiler;

    std::optional<slint::interpreter::ComponentDefinition> newDef;

    if (path == "") newDef = newCompiler.build_from_path(filePath + fileName);
    else newDef = newCompiler.build_from_path(path);

    if (!newDef.has_value())
    {
        EventHub::emit(Event::APP_ERROR, ErrorGui{"Hot reload failed: compile error"});
        for (auto& diag : newCompiler.diagnostics())
            fprintf(stderr, "[HotReload] → %s\n", diag.message.data());
        return;
    }

    // ComponentHandle is NOT optional, create() returns it directly
    auto newInst = newDef->create();

    // grab old geometry before touching anything
    auto oldPos  = instance.value()->window().position();

    // lock so trackingLoop doesn't touch modelManager mid-swap
    {
        std::lock_guard<std::mutex> lock(reloadMutex);
        UiRegistry::registerInstance(newInst);
        modelManager.emplace(newInst);
    }

    // re-wire UI event callbacks on new instance
    uiEventBridge.emplace(newInst, extManager, this);

    // show new BEFORE hiding oldanti-flicker
    newInst->show();
    instance.value()->hide();

    // restore geometry
    //this shit already runs on event thread
    //so this is fine
    const_cast<slint::Window&>(newInst->window()).set_position(oldPos);

    // swap
    {
        std::lock_guard<std::mutex> lock(reloadMutex);
        definition    = std::move(newDef);
        instance      = newInst;
        weak_instance = newInst;
    }

    // re-wire close handler
    #ifdef _WIN32
        instance.value()->window().on_close_requested([this]() -> slint::CloseRequestResponse {
            instance.value()->hide();
            return slint::CloseRequestResponse::KeepWindowShown;
        });
        slint::invoke_from_event_loop([]() {
            HWND hwnd = FindWindowW(nullptr, L"HPR");
            if (hwnd) {
                HICON hIconBig = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), IMAGE_ICON, 32, 32, LR_SHARED);
                HICON hIconSmall = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), IMAGE_ICON, 16, 16, LR_SHARED);
                if (hIconBig)   SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIconBig);
                if (hIconSmall) SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
            }
        });
    #else
        instance.value()->window().on_close_requested([this]() -> slint::CloseRequestResponse {
            this->hide();
            return slint::CloseRequestResponse::KeepWindowShown;
        });
    #endif
}

void HPRInterpreter::show()
{
    //tell to unpause
    {
        std::lock_guard<std::mutex> lock(pauseMutex);
        paused = false;
    }
    pauseCv.notify_one();

    auto weak = slint::ComponentWeakHandle<slint::interpreter::ComponentInstance>(instance.value());

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

void HPRInterpreter::quit()
{
    slint::invoke_from_event_loop([]()
                                  { slint::quit_event_loop(); });
}

void HPRInterpreter::hide()
{
    //pause the thread
    {
        std::lock_guard<std::mutex> lock(pauseMutex);
        paused = true;
    }

    auto weak = slint::ComponentWeakHandle<slint::interpreter::ComponentInstance>(instance.value());
    slint::invoke_from_event_loop([weak]()
    {
        if (auto handle = weak.lock())
            (*handle)->hide();
    });
}

void HPRInterpreter::trackingLoop()
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
    uint64_t totalTrackedTime; // For the bars
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
            std::unique_lock<std::mutex> lock(pauseMutex);
            
            pauseCv.wait(lock, [this] 
            { 
                return !paused || !running; 
            });
        }

        // If the app was literally CLOSED while hidden, break out immediately
        if (!running) break;

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
                    std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
                    //ALWAYS GET CURRENT LATEST WINDOW NO MATTER THE VIEW
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

            //Scoped mutex for hot reloading
            {
                std::lock_guard<std::mutex> lock(reloadMutex);
                modelManager.value().update_Interpreted(
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
                    std::lock_guard<std::mutex> lock(AppState::stateMutex);
                    
                    modelManager.value().showInsights_Interpreted(
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
                modelManager.value().showExtensions_Interpreted(extensionsCopy);
                modelManager.value().showFunStats_Interpreted(
                    getCpuUsage(),
                    getRamUsage(),
                    std::to_string(extensionsCopy.size()),
                    getThreadCount()
                );
            }

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
            int uiUpdateInterval = AppState::configManager.getConfig("ui-update-interval", 600);
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

void HPRInterpreter::run()
{

    //For saving my time
    auto &inst = instance.value();

    uiEventBridge.emplace(inst, extManager, this);

    #ifdef _WIN32
        auto weak_inst = slint::ComponentWeakHandle<slint::interpreter::ComponentInstance>(inst);
        inst->window().on_close_requested([this, weak_inst]() -> slint::CloseRequestResponse {
            if (auto locked = weak_inst.lock()) {
                (*locked)->hide();
            }
            return slint::CloseRequestResponse::KeepWindowShown;
        });
    #else
        inst->window().on_close_requested([this]() -> slint::CloseRequestResponse {
            this->hide();
            return slint::CloseRequestResponse::KeepWindowShown;
        });
    #endif

    bool headless = AppState::configManager.getConfig("headless-mode", false);
    if (!headless)
    {
        inst->show();
    }
    else
    {
        inst->show();
        auto weak = slint::ComponentWeakHandle<slint::interpreter::ComponentInstance>(inst);
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

    tracker = std::thread(&HPRInterpreter::trackingLoop, this);

    slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
    running = false; // safety net
}

bool HPRInterpreter::initialiseSlintUiPath()
{
    std::string tempPath;
    #ifdef _WIN32
        tempPath = std::getenv("APPDATA");
        tempPath += "/HPR/HPR_Config/ui/";
    #else
        const char* home = std::getenv("HOME");
        if (!home) throw std::runtime_error("HOME env var not set");
        tempPath = home;
        tempPath += "/.config/HPR/ui/";
    #endif

    std::filesystem::create_directories(tempPath);
    filePath = tempPath;

    std::ifstream file(filePath + fileName);

    if (!file.is_open())
    {
        std::cerr << "Warning: " << fileName << "  not found at " << filePath
                  << ". Closing HPR .\n";
        Logger::log("Warning: " + fileName + " not found at " + filePath + ". Closing HPR.");
        return false;
    }
    return true;
}