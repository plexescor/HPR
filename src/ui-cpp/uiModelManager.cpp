#include "uiModelManager.hpp"
#include "aliasManager.hpp"
#include "timeUtils.hpp"

#include <map>
#include <cstdint>

// Slint stuff
#include "app-window.h"
#include <slint.h>
#include <slint-interpreter.h>

UiModelManager::UiModelManager(slint::ComponentHandle<MainWindow> &ui_handle) : ui(ui_handle)
{
    this->ui = ui;
    // make shared so they aint null and no thing crashes
    timeLogModel = std::make_shared<slint::VectorModel<TimeLog>>();
    switchHistoryModel = std::make_shared<slint::VectorModel<SwitchHistory>>();

    slint::ComponentWeakHandle<MainWindow> weak(ui.value());
    slint::invoke_from_event_loop([weak, this]()
    {
        if (auto handle = weak.lock()) {
            //set in ui
            (*handle)->set_timePerApp_S(timeLogModel);
            (*handle)->set_switchHistory_S(switchHistoryModel);
        } 
    });
}

// second constructor for interpreter mode
UiModelManager::UiModelManager(slint::ComponentHandle<slint::interpreter::ComponentInstance> &ui_handle) : ui_interp(ui_handle)
{
    timeLogModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
    switchHistoryModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();

    slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> weak(ui_interp.value());
    slint::invoke_from_event_loop([weak, this]()
    {
        if (auto handle = weak.lock())
        {
            (*handle)->set_property("timePerApp_S",    slint::interpreter::Value(timeLogModel_interp));
            (*handle)->set_property("switchHistory_S", slint::interpreter::Value(switchHistoryModel_interp));
        }
    });
}

UiModelManager::~UiModelManager()
{
}

void UiModelManager::update(const std::map<std::string, long> &rawTimeLog,
                            const std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> &rawHistory,
                            std::string &currentWindowName,
                            long &totalTrackedTime,
                            AliasManager &aliasManager)
{

    currentWindowName = aliasManager.getAlias(currentWindowName);
    //----------------------TIME LOG-----------------------------------------------

    // make a middle man translatedTimeLog with correct aliases and push rawTimeLog
    std::map<std::string, long> translatedTimeLog;

    for (const auto &[raw, duration] : rawTimeLog)
    {
        translatedTimeLog[aliasManager.getAlias(raw)] += duration;
        totalTrackedTime += duration;
    }

    // create a vector of slint's TimeLog struct
    // Use pretty names
    std::vector<TimeLog> slintVec_TimeLog;
    for (const auto &[name, duration] : translatedTimeLog)
    {
        slintVec_TimeLog.push_back({slint::SharedString(name),
                                    slint::SharedString(formatTime_HHMMSS(duration)), // The HH:MM:SS string
                                    (int)duration});
    }

    // Sort so most used comes at top
    std::sort(slintVec_TimeLog.begin(), slintVec_TimeLog.end(), [](const TimeLog &a, const TimeLog &b)
              { return a.duration_i > b.duration_i; });

    //----------------------------------SWITCH HISTORY---------------------------------------

    // Middle man
    // From Window, to window, timestamp in str
    // std::map<std::pair<std::string, std::string>, std::string> translatedSwitchHistory;

    // For max value specifically
    struct TempSwitchHistory
    {
        std::string from;
        std::string to;
        uint64_t maxVal;
    };

    std::vector<TempSwitchHistory> tempSwitchVec;
    tempSwitchVec.reserve(rawHistory.size());

    for (const auto &[route, timeStampS] : rawHistory)
    {
        if (timeStampS.empty()) continue; // Safety guard
           
        const auto &[from, to] = route;

        const auto maxVal = *std::max_element(timeStampS.begin(), timeStampS.end());
        tempSwitchVec.push_back({from, to, maxVal});
    }

    // sort so latest one comes at top
    std::sort(tempSwitchVec.begin(), tempSwitchVec.end(), [](const TempSwitchHistory &a, const TempSwitchHistory &b)
              { return a.maxVal > b.maxVal; });

    // create a vector of slint's switch History struct
    // Use pretty names
    std::vector<SwitchHistory> slintVec_SwitchHistory;

    for (const auto &item : tempSwitchVec)
    {
        slintVec_SwitchHistory.push_back(
            SwitchHistory{
                slint::SharedString(aliasManager.getAlias(item.from)),
                slint::SharedString(aliasManager.getAlias(item.to)),
                slint::SharedString(convertToTime_HHMMSS_12(item.maxVal))});
    }

    //-----------------Actually pushing changes to slint for rendering-----------------------
    if (!ui.has_value()) {
        return; //skip update if ui handle aint ready
    }
    slint::ComponentWeakHandle<MainWindow> weak(ui.value());
    slint::invoke_from_event_loop([weak, slintVec_TimeLog, slintVec_SwitchHistory, totalTrackedTime, currentWindowName, this]()
    {
        if (auto handle = weak.lock()) {
                (*handle)->set_windowName_S(slint::SharedString(currentWindowName));

                (*handle)->set_trackedTime_S(totalTrackedTime);
                // Surgical update to prevent layout panics during resize/maximize
                auto syncModel = [](auto model, const auto& vec) 
                {
                    size_t existing_count = model->row_count();
                    size_t new_count = vec.size();
                    size_t min_count = (std::min)(existing_count, new_count);

                    // Update existing rows
                    for (size_t i = 0; i < min_count; ++i) 
                    {
                        model->set_row_data(i, vec[i]);
                    }

                    // Remove excess rows from the end
                    while (model->row_count() > new_count) 
                    {
                        model->erase(model->row_count() - 1);
                    }

                    // Add new rows
                    for (size_t i = existing_count; i < new_count; ++i) 
                    {
                        model->push_back(vec[i]);
                    }
                };

                syncModel(timeLogModel, slintVec_TimeLog);
                syncModel(switchHistoryModel, slintVec_SwitchHistory);
            }
    });
}

void UiModelManager::update_Interpreted(const std::map<std::string, long> &rawTimeLog,
                            const std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> &rawHistory,
                            std::string &currentWindowName,
                            long &totalTrackedTime,
                            AliasManager &aliasManager)
{
    currentWindowName = aliasManager.getAlias(currentWindowName);

    //----------------------TIME LOG-----------------------------------------------

    std::map<std::string, long> translatedTimeLog;

    for (const auto &[raw, duration] : rawTimeLog)
    {
        translatedTimeLog[aliasManager.getAlias(raw)] += duration;
        totalTrackedTime += duration;
    }

    // interpreter uses Value, not typed structs
    std::vector<slint::interpreter::Value> slintVec_TimeLog;

    for (const auto &[name, duration] : translatedTimeLog)
    {
        slint::interpreter::Struct entry;
        entry.set_field("name",       slint::interpreter::Value(slint::SharedString(name)));
        entry.set_field("duration",   slint::interpreter::Value(slint::SharedString(formatTime_HHMMSS(duration))));
        entry.set_field("duration_i", slint::interpreter::Value((double)duration));
        slintVec_TimeLog.push_back(slint::interpreter::Value(entry));
    }

    // Sort by duration_i descending
    std::sort(slintVec_TimeLog.begin(), slintVec_TimeLog.end(),
    [](const slint::interpreter::Value &a, const slint::interpreter::Value &b)
    {
        auto sa = a.to_struct();
        auto sb = b.to_struct();

        if (sa && sb) 
        {
            // get_field returns an optional Value, then we convert to number, then dereference
            double da = sa->get_field("duration_i").value_or(slint::interpreter::Value(0.0)).to_number().value_or(0.0);
            double db = sb->get_field("duration_i").value_or(slint::interpreter::Value(0.0)).to_number().value_or(0.0);
            return da > db;
        }
        return false;
    });
    //----------------------------------SWITCH HISTORY---------------------------------------

    struct TempSwitchHistory
    {
        std::string from;
        std::string to;
        uint64_t maxVal;
    };

    std::vector<TempSwitchHistory> tempSwitchVec;

    for (const auto &[route, timeStampS] : rawHistory)
    {
        if (timeStampS.empty()) continue;
        const auto &[from, to] = route;
        const auto maxVal = *std::max_element(timeStampS.begin(), timeStampS.end());
        tempSwitchVec.push_back({from, to, maxVal});
    }

    std::sort(tempSwitchVec.begin(), tempSwitchVec.end(),
        [](const TempSwitchHistory &a, const TempSwitchHistory &b)
        { return a.maxVal > b.maxVal; });

    std::vector<slint::interpreter::Value> slintVec_SwitchHistory;

    for (const auto &item : tempSwitchVec)
    {
        slint::interpreter::Struct entry;
        entry.set_field("fromWindow",      slint::interpreter::Value(slint::SharedString(aliasManager.getAlias(item.from))));
        entry.set_field("toWindow",        slint::interpreter::Value(slint::SharedString(aliasManager.getAlias(item.to))));
        entry.set_field("maxTimeStamp", slint::interpreter::Value(slint::SharedString(convertToTime_HHMMSS_12(item.maxVal))));
        slintVec_SwitchHistory.push_back(slint::interpreter::Value(entry));
    }

    //-----------------Actually pushing changes to slint for rendering-----------------------

    slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> weak(ui_interp.value());
    slint::invoke_from_event_loop([weak, slintVec_TimeLog, slintVec_SwitchHistory, totalTrackedTime, currentWindowName, this]()
    {
        if (auto handle = weak.lock())
        {
            (*handle)->set_property("windowName_S",   slint::interpreter::Value(slint::SharedString(currentWindowName)));
            (*handle)->set_property("trackedTime_S",  slint::interpreter::Value((double)totalTrackedTime));

            auto syncModel = [](
                std::shared_ptr<slint::VectorModel<slint::interpreter::Value>> model,
                const std::vector<slint::interpreter::Value>& vec)
            {
                size_t existing_count = model->row_count();
                size_t new_count = vec.size();
                size_t min_count = (std::min)(existing_count, new_count);

                for (size_t i = 0; i < min_count; ++i)
                    model->set_row_data(i, vec[i]);

                while (model->row_count() > new_count)
                    model->erase(model->row_count() - 1);

                for (size_t i = existing_count; i < new_count; ++i)
                    model->push_back(vec[i]);
            };

            syncModel(timeLogModel_interp, slintVec_TimeLog);
            syncModel(switchHistoryModel_interp, slintVec_SwitchHistory);
        }
    });
}
