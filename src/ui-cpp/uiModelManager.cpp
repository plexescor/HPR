#include "uiModelManager.hpp"
#include "aliasManager.hpp"
#include "timeUtils.hpp"

#include <map>
#include <cstdint>

// Slint stuff
#include "app-window.h"
#include <slint.h>

UiModelManager::UiModelManager(slint::ComponentHandle<MainWindow> &ui) : ui(ui)
{
    this->ui = ui;
    // make shared so they aint null and no thing crashes
    timeLogModel = std::make_shared<slint::VectorModel<TimeLog>>();
    switchHistoryModel = std::make_shared<slint::VectorModel<SwitchHistory>>();

    slint::ComponentWeakHandle<MainWindow> weak(ui);
    slint::invoke_from_event_loop([weak, this]()
    {
        if (auto handle = weak.lock()) {
            //set in ui
            (*handle)->set_timePerApp_S(timeLogModel);
            (*handle)->set_switchHistory_S(switchHistoryModel);
        } 
    });
}
UiModelManager::~UiModelManager()
{
}

void UiModelManager::update(const std::map<std::string, long> &rawTimeLog,
                            const std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> &rawHistory,
                            const std::string &currentWindowName,
                            long &totalTrackedTime,
                            AliasManager &aliasManager)
{
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
    std::map<std::pair<std::string, std::string>, std::string> translatedSwitchHistory;

    // For max value specifically
    struct TempSwitchHistory
    {
        std::string from;
        std::string to;
        uint64_t maxVal;
    };

    std::vector<TempSwitchHistory> tempSwitchVec;
    tempSwitchVec.reserve(translatedSwitchHistory.size());

    for (const auto &[route, timeStampS] : rawHistory)
    {
        if (timeStampS.empty())
            continue; // Safety guard
        const auto &[from, to] = route;

        const auto maxVal = *std::max_element(timeStampS.begin(), timeStampS.end());
        tempSwitchVec.push_back({from, to, maxVal});
    }

    // sort so latest one comes at top
    std::sort(tempSwitchVec.begin(), tempSwitchVec.end(), [](const TempSwitchHistory &a, const TempSwitchHistory &b)
              { return a.maxVal > b.maxVal; });

    // create a vector of slint's TimeLog struct
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
    slint::ComponentWeakHandle<MainWindow> weak(ui);
    slint::invoke_from_event_loop([weak, slintVec_TimeLog, slintVec_SwitchHistory, totalTrackedTime, currentWindowName, this]()
    {
        if (auto handle = weak.lock()) {
                (*handle)->set_windowName_S(slint::SharedString(currentWindowName));

                (*handle)->set_trackedTime_S(totalTrackedTime);
                // Surgical update to prevent layout panics during resize/maximize
                auto syncModel = [](auto model, const auto& vec) {
                    size_t existing_count = model->row_count();
                    size_t new_count = vec.size();
                    size_t min_count = (std::min)(existing_count, new_count);

                    // Update existing rows
                    for (size_t i = 0; i < min_count; ++i) {
                        model->set_row_data(i, vec[i]);
                    }

                    // Remove excess rows from the end
                    while (model->row_count() > new_count) {
                        model->erase(model->row_count() - 1);
                    }

                    // Add new rows
                    for (size_t i = existing_count; i < new_count; ++i) {
                        model->push_back(vec[i]);
                    }
                };

                syncModel(timeLogModel, slintVec_TimeLog);
                syncModel(switchHistoryModel, slintVec_SwitchHistory);
            }
    });
}
