#include "HPRInterpreter.hpp"

#include <slint-interpreter.h>

#include "appState.hpp"
#include "getCurrentWindow.hpp"
#include "timeUtils.hpp"
#include "aliasManager.hpp"
#include "uiEventBridge_Interpreted.hpp"

#include <thread>
#include <atomic>
#include <optional>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
    #include "Windows.h"
#endif

HPRInterpreter::HPRInterpreter()
{
    if (!initialiseSlintUiPath()) exit(1);

    definition = compiler.build_from_path(filePath + fileName);

    if (!definition.has_value()) {
        for (auto& diag : compiler.diagnostics())
            fprintf(stderr, "  → %s\n", diag.message.data());
        exit(1);
    }

    instance = definition->create();
    weak_instance = instance.value();

    modelManager.emplace(instance.value());
    #ifdef __linux__
        slint::set_xdg_app_id("HPR"); // So it has a class in hyprland
    #endif
}

HPRInterpreter::~HPRInterpreter()
{
    running = false;
    if (tracker.joinable())
        tracker.join(); // Speaks for itself
}

void HPRInterpreter::show()
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
    
    auto weak = slint::ComponentWeakHandle<slint::interpreter::ComponentInstance>(instance.value());

    slint::invoke_from_event_loop([weak]()
                                  {
        if (auto handle = weak.lock()) 
        {
            (*handle)->show();
        } });
}

void HPRInterpreter::quit()
{
    slint::invoke_from_event_loop([]()
                                  { slint::quit_event_loop(); });
}

void HPRInterpreter::hide()
{
    slint::invoke_from_event_loop([this]()
                                  { instance.value()->hide(); });
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

            modelManager.value().update(
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

void HPRInterpreter::run()
{
    //For saving my time
    auto &inst = instance.value();

    UiEventBridge_Interpreted uiEventBridge(inst);

    #ifdef _WIN32
        inst->window().on_close_requested([this, inst]() -> slint::CloseRequestResponse {
            inst->hide();
            return slint::CloseRequestResponse::KeepWindowShown;
        });
    #else
        inst->window().on_close_requested([this]() -> slint::CloseRequestResponse {
            running = false;
            slint::quit_event_loop();
            return slint::CloseRequestResponse::KeepWindowShown;
        });
    #endif

    inst->show();

    tracker = std::thread(&HPRInterpreter::trackingLoop, this);

    slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
    running = false; // safety net
}

bool HPRInterpreter::initialiseSlintUiPath()
{
    std::string tempPath;
    #ifdef _WIN32
        tempPath = std::getenv("APPDATA");
        tempPath += "/HPR/HPR_Config/instance/";
    #else
        const char* home = std::getenv("HOME");
        if (!home) throw std::runtime_error("HOME env var not set");
        tempPath = home;
        tempPath += "/.config/HPR/instance/";
    #endif

    std::filesystem::create_directories(tempPath);
    filePath = tempPath;

    std::ifstream file(filePath + fileName);

    if (!file.is_open())
    {
        std::cerr << "Warning: " << fileName << "  not found at " << filePath
                  << ". Closing HPR .\n";
        return false;
    }
    return true;
}