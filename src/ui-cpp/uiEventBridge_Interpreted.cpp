#include "uiEventBridge_Interpreted.hpp"
#include "appEvents.hpp"
#include "appState.hpp"
#include <slint-interpreter.h>

UiEventBridge_Interpreted::UiEventBridge_Interpreted(
    slint::ComponentHandle<slint::interpreter::ComponentInstance>& ui)
{
    loadDbSingularId = EventHub::connect(Event::HISTORY_LOADED_SINGULAR, 
        [this](EventData data) {
            this->showHistoricalDataSingular();
        });

    ui->set_callback("loadHistoricalData_Singular",
        [](auto args) -> slint::interpreter::Value 
        {
            // args is std::span<const slint::interpreter::Value>
            if (args.size() > 0) {
                // Safely convert the interpreter value to an optional SharedString
                auto opt_str = args[0].to_string();
                
                if (opt_str.has_value()) {
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
}

UiEventBridge_Interpreted::~UiEventBridge_Interpreted()
{
    EventHub::disconnect(Event::HISTORY_LOADED_SINGULAR, loadDbSingularId);
}

void UiEventBridge_Interpreted::showHistoricalDataSingular()
{
    //Make the current app state historical
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.currentView = AppState::CurrentView::HISTORICAL_SINGULAR;
}

void UiEventBridge_Interpreted::showLiveData()
{
    //Make the current app state historical
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.currentView = AppState::CurrentView::LIVE;
}