#include "uiEventBridge.hpp"
#include "appEvents.hpp"
#include "appState.hpp"

// Slint stuff
#include "app-window.h"
#include <slint.h>

#ifdef _WIN32
    #include <windows.h>
#endif

//CONSTRUCTOR FOR **COMPILED** UI
UiEventBridge::UiEventBridge(slint::ComponentHandle<MainWindow>& ui)
{

    //Connect to event hub
    init();

    ui->on_loadHistoricalData_Singular([](slint::SharedString dateFromUi) 
    {
        //convert to cpp string
        std::string requestedDate = std::string(dateFromUi);
        
        //Emit the signal that we need to load data
        EventHub::emit(
            Event::LOAD_DATABASE_SINGULAR, 
            DatabaseDate_Singular{requestedDate}
        );
    });

    ui->on_loadLiveData([this]() 
    {
        //No need for event hub
        this->showLiveData();
    });

    ui->on_tabViewClicked([this]() 
    {
        this->tabViewClicked();
    });

    ui->on_siteViewClicked([this]() 
    {
        this->siteViewClicked();
    });

    ui->on_openKofi([this]()
    {
        //Open webbrowser
        #ifdef _WIN32
            ShellExecuteA(nullptr, "open", "https://ko-fi.com/plexescor", nullptr, nullptr, SW_SHOWNORMAL);
        #else
            int lol = system("xdg-open https://ko-fi.com/plexescor &");
        #endif
    });
}

//CONSTRUCTOR FOR **INTERPRETED** UI
UiEventBridge::UiEventBridge(
    slint::ComponentHandle<slint::interpreter::ComponentInstance>& ui)
{
    //Connect to event hub
    init();

    ui->set_callback("loadHistoricalData_Singular",
        [](auto args) -> slint::interpreter::Value 
        {
            // args is std::span<const slint::interpreter::Value>
            if (args.size() > 0) 
            {
                // Safely convert the interpreter value to an optional SharedString
                auto opt_str = args[0].to_string();
                
                if (opt_str.has_value()) 
                {
                    // Dereference the optional to get the SharedString, then cast to std::string
                    std::string requestedDate = std::string(opt_str.value());

                    EventHub::emit(
                        Event::LOAD_DATABASE_SINGULAR,
                        DatabaseDate_Singular{requestedDate}
                    );
                }
            }
            return slint::interpreter::Value(); // void return
        });
    ui->set_callback("loadLiveData", [this](auto args) -> slint::interpreter::Value
    {
        //No need for event hub
        this->showLiveData();
        return slint::interpreter::Value(); // void return
    });

    ui->set_callback("tabViewClicked", [this](auto args) -> slint::interpreter::Value
    {
        //No need for event hub
        this->tabViewClicked();
        return slint::interpreter::Value(); // void return
    });

    ui->set_callback("siteViewClicked", [this](auto args) -> slint::interpreter::Value
    {
        //No need for event hub
        this->siteViewClicked();
        return slint::interpreter::Value(); // void return
    });

    ui->set_callback("openKofi", [this](auto args) -> slint::interpreter::Value
    {
        //open browser
        #ifdef _WIN32
            ShellExecuteA(nullptr, "open", "https://ko-fi.com/plexescor", nullptr, nullptr, SW_SHOWNORMAL);
        #else
            int idc = system("xdg-open https://ko-fi.com/plexescor &");
        #endif
        return slint::interpreter::Value(); // void return
    });
}


UiEventBridge::~UiEventBridge()
{
    EventHub::disconnect(Event::HISTORY_LOADED_SINGULAR, loadDbSingularId);
}

void UiEventBridge::init() {
    // -----------------------Connecting to the Event Hub ----------------------------------------
    loadDbSingularId = EventHub::connect(Event::HISTORY_LOADED_SINGULAR, [this](EventData data)
    {
        this->showHistoricalDataSingular();
    });
}

void UiEventBridge::showHistoricalDataSingular()
{
    //Make the current app state historical
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.currentView = AppState::CurrentView::HISTORICAL_SINGULAR;
}

void UiEventBridge::showLiveData()
{
    //Make the current app state live
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.currentView = AppState::CurrentView::LIVE;
}

void UiEventBridge::tabViewClicked()
{
    //Make the current app state live
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.useTabView = true;
}

void UiEventBridge::siteViewClicked()
{
    //Make the current app state live
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.useTabView = false;
}