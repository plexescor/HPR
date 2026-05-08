#include "uiEventBridge.hpp"
#include "appEvents.hpp"
#include "appState.hpp"

// Slint stuff
#include "app-window.h"
#include <slint.h>

UiEventBridge::UiEventBridge(slint::ComponentHandle<MainWindow>& ui)
{
    // -----------------------Connecting to the Event Hub ----------------------------------------
    loadDbSingularId = EventHub::connect(Event::HISTORY_LOADED_SINGULAR, [this](EventData data)
    {
        this->showHistoricalDataSingular();
    });

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
}

UiEventBridge::~UiEventBridge()
{
    EventHub::disconnect(Event::HISTORY_LOADED_SINGULAR, loadDbSingularId);
}

void UiEventBridge::showHistoricalDataSingular()
{
    //Make the current app state historical
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.currentView = AppState::CurrentView::HISTORICAL_SINGULAR;
}

void UiEventBridge::showLiveData()
{
    //Make the current app state historical
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.currentView = AppState::CurrentView::LIVE;
}