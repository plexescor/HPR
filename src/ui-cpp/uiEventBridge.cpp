#include "uiEventBridge.hpp"
#include "appEvents.hpp"

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
}

UiEventBridge::~UiEventBridge()
{
    EventHub::disconnect(Event::HISTORY_LOADED_SINGULAR, loadDbSingularId);
}

void UiEventBridge::showHistoricalDataSingular()
{
    //No shit
}