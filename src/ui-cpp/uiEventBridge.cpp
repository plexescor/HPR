#include "uiEventBridge.hpp"
#include "appEvents.hpp"
#include "appState.hpp"
#include "extensionManager.hpp"
#include "HPRInterpreter.hpp"
#include "limitsManager.hpp"

// Slint stuff
#include "app-window.h"
#include <slint.h>

#ifdef _WIN32
    #include <windows.h>
#endif

//CONSTRUCTOR FOR **COMPILED** UI
UiEventBridge::UiEventBridge(slint::ComponentHandle<MainWindow>& ui,  ExtensionManager* extMgr, HPRInterpreter* interpreter)
{
    if (extMgr)
        this->extManager = extMgr;

    if (interpreter)
        this->interpreter = interpreter;


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

    ui->on_rawViewClicked([this]() 
    {
        this->rawViewClicked();
    });

    ui->on_filterViewClicked([this]() 
    {
        this->filterViewClicked();
    });

    if (extManager)
    {
        ui->on_refreshExtensions([this]() 
        {
            extManager->refresh();
        });
        ui->on_disableExtension([this](slint::SharedString author, slint::SharedString name) 
        {
            extManager->unloadExtension(std::string(author), std::string(name));
        });
        ui->on_reloadExtension([this](slint::SharedString author, slint::SharedString name) 
        {
            extManager->reloadExtension(std::string(author), std::string(name));
        });
    }

    //no need to do shit in compiled mode

    // if (interpreter)
    // {
    //     ui->on_reloadUi([this]()
    //     {   
    //         slint::invoke_from_event_loop([this]() 
    //         {
    //             this->interpreter->reload();
    //         });
    //     });
    // }

    ui->on_setLimit([](slint::SharedString appName, int minutes) 
    {
        LimitsManager::setLimit(std::string(appName), minutes);
    });

    ui->on_setGoal([](slint::SharedString appName, int minutes) 
    {
        LimitsManager::setGoal(std::string(appName), minutes);
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

    ui->on_openReleases([this]()
    {
        //Open webbrowser
        #ifdef _WIN32
            ShellExecuteA(nullptr, "open", "https://github.com/plexescor/HPR/releases", nullptr, nullptr, SW_SHOWNORMAL);
        #else
            int lol = system("xdg-open https://github.com/plexescor/HPR/releases &");
        #endif
    });

    ui->on_openIssues([this]()
    {
        //Open webbrowser
        #ifdef _WIN32
            ShellExecuteA(nullptr, "open", "https://github.com/plexescor/HPR/issues", nullptr, nullptr, SW_SHOWNORMAL);
        #else
            int lol = system("xdg-open https://github.com/plexescor/HPR/issues &");
        #endif
    });
}

//CONSTRUCTOR FOR **INTERPRETED** UI
UiEventBridge::UiEventBridge(
    slint::ComponentHandle<slint::interpreter::ComponentInstance>& ui,  ExtensionManager* extMgr, HPRInterpreter* interpreter)
{
    if (extMgr)
        this->extManager = extMgr;

    if (interpreter)
        this->interpreter = interpreter;

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

    ui->set_callback("rawViewClicked", [this](auto args) -> slint::interpreter::Value
    {
        //No need for event hub
        this->rawViewClicked();
        return slint::interpreter::Value(); // void return
    });

    ui->set_callback("filterViewClicked", [this](auto args) -> slint::interpreter::Value
    {
        //No need for event hub
        this->filterViewClicked();
        return slint::interpreter::Value(); // void return
    });

    if (interpreter)
    {
        ui->set_callback("reloadUi", [this](auto args) -> slint::interpreter::Value
        {   
            slint::invoke_from_event_loop([this]() 
            {
                this->interpreter->reload();
            });
            return slint::interpreter::Value(); // void return
        });
    }

    if (extManager)
    {
        ui->set_callback("refreshExtensions", [this](auto args) -> slint::interpreter::Value
        {
            extManager->refresh();
            return slint::interpreter::Value();
        });

        ui->set_callback("disableExtension", [this](auto args) -> slint::interpreter::Value
        {
            if (args.size() > 1)
            {
                auto opt_author = args[0].to_string();
                auto opt_name = args[1].to_string();
                if (opt_author.has_value() && opt_name.has_value())
                    extManager->unloadExtension(std::string(opt_author.value()), std::string(opt_name.value()));
            }
            return slint::interpreter::Value();
        });

        ui->set_callback("reloadExtension", [this](auto args) -> slint::interpreter::Value
        {
            if (args.size() > 1)
            {
                auto opt_author = args[0].to_string();
                auto opt_name = args[1].to_string();
                if (opt_author.has_value() && opt_name.has_value())
                    extManager->reloadExtension(std::string(opt_author.value()), std::string(opt_name.value()));
            }
            return slint::interpreter::Value();
        });
    }

    ui->set_callback("setLimit", [](auto args) -> slint::interpreter::Value 
    {
        if (args.size() > 1) 
        {
            auto opt_name = args[0].to_string();
            auto opt_mins = args[1].to_number();
            if (opt_name.has_value() && opt_mins.has_value()) 
            {
                LimitsManager::setLimit(std::string(opt_name.value()), (int)opt_mins.value());
            }
        }
        return slint::interpreter::Value();
    });

    ui->set_callback("setGoal", [](auto args) -> slint::interpreter::Value 
    {
        if (args.size() > 1) 
        {
            auto opt_name = args[0].to_string();
            auto opt_mins = args[1].to_number();
            if (opt_name.has_value() && opt_mins.has_value()) 
            {
                LimitsManager::setGoal(std::string(opt_name.value()), (int)opt_mins.value());
            }
        }
        return slint::interpreter::Value();
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

    ui->set_callback("openReleases", [this](auto args) -> slint::interpreter::Value
    {
        //open browser
        #ifdef _WIN32
            ShellExecuteA(nullptr, "open", "https://github.com/plexescor/HPR/releases", nullptr, nullptr, SW_SHOWNORMAL);
        #else
            int idc = system("xdg-open https://github.com/plexescor/HPR/releases &");
        #endif
        return slint::interpreter::Value(); // void return
    });

    ui->set_callback("openIssues", [this](auto args) -> slint::interpreter::Value
    {
        //open browser
        #ifdef _WIN32
            ShellExecuteA(nullptr, "open", "https://github.com/plexescor/HPR/issues", nullptr, nullptr, SW_SHOWNORMAL);
        #else
            int idc = system("xdg-open https://github.com/plexescor/HPR/issues &");
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
    EventHub::emit(Event::LOAD_LIVE_DATA); // Tell everyone we need live data, so they can prepare it before we switch the view
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

void UiEventBridge::filterViewClicked()
{
    //Make the current app state live
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.isRawProjectView = false;
}

void UiEventBridge::rawViewClicked()
{
    //Make the current app state live
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.isRawProjectView = true;
}