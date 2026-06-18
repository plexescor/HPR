#include "uiModelManager.hpp"
#include "logger.hpp"
#include "aliasManager.hpp"
#include "timeUtils.hpp"
#include "appState.hpp"
#include "limitsManager.hpp"
#include "timelineManager.hpp"


#include <map>
#include <cstdint>
#include <mutex>

// Slint stuff
#include "app-window.h"
#include <slint.h>
#include <slint-interpreter.h>

UiModelManager::UiModelManager(slint::ComponentHandle<MainWindow> &ui_handle) : ui(ui_handle)
{
    this->ui = ui;
    // make shared so they aint null and no thing crashes
    timeLogModel = std::make_shared<slint::VectorModel<TimeLog>>();
    timeLogModelTab = std::make_shared<slint::VectorModel<TimeLog_Tab>>();
    timeLogModelProject = std::make_shared<slint::VectorModel<TimeLog_Project>>();
    switchHistoryModel = std::make_shared<slint::VectorModel<SwitchHistory>>();
    extensionsModel = std::make_shared<slint::VectorModel<LoadedExtension_S>>();
    rawAppsModel = std::make_shared<slint::VectorModel<AppGoalData>>();
    timelineBlocksModel = std::make_shared<slint::VectorModel<TimelineBlock>>();
    timelineMarkersModel = std::make_shared<slint::VectorModel<TimelineMarker>>();

    slint::ComponentWeakHandle<MainWindow> weak(ui.value());
    slint::invoke_from_event_loop([weak, this]()
    {
        if (auto handle = weak.lock()) {
            //set in ui
            (*handle)->set_timePerApp_S(timeLogModel);
            (*handle)->set_timePerTab_S(timeLogModelTab);
            (*handle)->set_timePerProject_S(timeLogModelProject);
            (*handle)->set_switchHistory_S(switchHistoryModel);
            (*handle)->set_loadedExtensions_S(extensionsModel);
            (*handle)->set_rawApps_S(rawAppsModel);
            (*handle)->set_timelineBlocks_S(timelineBlocksModel);
            (*handle)->set_timelineMarkers_S(timelineMarkersModel);
        } 
    });

}

// second constructor for interpreter mode
UiModelManager::UiModelManager(slint::ComponentHandle<slint::interpreter::ComponentInstance> &ui_handle) : ui_interp(ui_handle)
{
    timeLogModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
    timeLogModelTab_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
    timeLogModelProject_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
    switchHistoryModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
    extensionsModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
    rawAppsModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
    timelineBlocksModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
    timelineMarkersModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();


    slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> weak(ui_interp.value());
    slint::invoke_from_event_loop([weak, this]()
    {
        if (auto handle = weak.lock())
        {
            (*handle)->set_property("timePerApp_S",    slint::interpreter::Value(timeLogModel_interp));
            (*handle)->set_property("timePerTab_S",    slint::interpreter::Value(timeLogModelTab_interp));
            (*handle)->set_property("timePerProject_S", slint::interpreter::Value(timeLogModelProject_interp));
            (*handle)->set_property("switchHistory_S", slint::interpreter::Value(switchHistoryModel_interp));
            (*handle)->set_property("loadedExtensions_S", slint::interpreter::Value(extensionsModel_interp));
            (*handle)->set_property("rawApps_S", slint::interpreter::Value(rawAppsModel_interp));
            (*handle)->set_property("timelineBlocks_S", slint::interpreter::Value(timelineBlocksModel_interp));
            (*handle)->set_property("timelineMarkers_S", slint::interpreter::Value(timelineMarkersModel_interp));
        }
    });

}

UiModelManager::~UiModelManager()
{
}

void UiModelManager::update(const std::map<std::string, uint64_t> &rawTimeLog,
                            const std::map<std::string, uint64_t> &rawTimeLog_Tab,
                            const std::map<std::string, uint64_t> &rawTimeLog_Project,
                            const std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> &rawHistory,
                            std::string &currentWindowName,
                            uint64_t &totalTrackedTime,
                            uint64_t &totalTrackedTime_Tab,
                            uint64_t &totalTrackedTime_Project,
                            AliasManager &aliasManager)
{

    currentWindowName = aliasManager.getAlias(currentWindowName);
    //----------------------TIME LOG-----------------------------------------------

    // make a middle man translatedTimeLog with correct aliases and push rawTimeLog
    std::map<std::string, uint64_t> translatedTimeLog;

    for (const auto &[raw, duration] : rawTimeLog)
    {
        translatedTimeLog[aliasManager.getAlias(raw)] += duration;
        totalTrackedTime += duration;
    }

    //TAB
    bool isTabView;
    std::map<std::string, uint64_t> translatedTimeLog_Tab;
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        isTabView= AppState::state.useTabView;
    }

    //if tab view is on, we show raw data without aliasing, because it is per *tab* and not per site, so aliasing would just mess things up. If tab view is off, we alias as normal
    if (isTabView)
    {
        for (const auto &[raw, duration] : rawTimeLog_Tab)
        {
            translatedTimeLog_Tab[raw] += duration;
            totalTrackedTime_Tab += duration;
        }
    }
    else
    {
        for (const auto &[raw, duration] : rawTimeLog_Tab)
        {
            translatedTimeLog_Tab[aliasManager.getAlias_Tab(raw)] += duration;
            totalTrackedTime_Tab += duration;
        }
    }
    
    //Projects
    bool isRawView;
    std::map<std::string, uint64_t> translatedTimeLog_Project;
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        isRawView= AppState::state.isRawProjectView;
    }

    //if its raw view, just raw dog it
    if (isRawView)
    {
        for (const auto &[raw, duration] : rawTimeLog_Project)
        {
            translatedTimeLog_Project[raw] += duration;
            totalTrackedTime_Project += duration;
        }
    }
    //Parse it first and then find an aliases if present
    else 
    {
        //How does this parse? interesting question
        //Lets take an example: test.cpp - HPR - Visual Studio Code is the window title
        //what it will do is first check if it ends with " - Visual Studio Code", if it does, it will remove that part, so now we have "test.cpp - HPR"
        //Then it will find the last occurrence of " - ", which is the separator between project
        //keep in mind it finds " - " and not just "-", because project names can have dashes in them, but the separator is always " - "
        //no person uses " - " in their project names, that would be shit, so this is a safe separator to use"
        for (const auto &[raw, duration] : rawTimeLog_Project)
        {
            std::string middleMan;

            const std::string suffix = " - Visual Studio Code";
            std::string stripped = raw;
            
            if (stripped.size() >= suffix.size() && 
                stripped.substr(stripped.size() - suffix.size()) == suffix)
                stripped = stripped.substr(0, stripped.size() - suffix.size());
            
            // rfind last " - "
            size_t pos = stripped.rfind(" - ");
            if (pos == std::string::npos)
                middleMan = stripped; // no separator, return raw
            else
                middleMan = stripped.substr(pos + 3);

            translatedTimeLog_Project[aliasManager.getAlias_Project(middleMan)] += duration;
            totalTrackedTime_Project += duration;
        }
    }


    // create a vector of slint's TimeLog struct
    // Use pretty names
    std::vector<TimeLog> slintVec_TimeLog;
    for (const auto &[name, duration] : translatedTimeLog)
    {
        slintVec_TimeLog.push_back({slint::SharedString(name),
                                    slint::SharedString(formatTime_HHMMSS(duration)), // The HH:MM:SS string
                                    (float)duration});
    }

    std::vector<TimeLog_Tab> slintVec_TimeLog_Tab;
    for (const auto &[name, duration] : translatedTimeLog_Tab)
    {
        slintVec_TimeLog_Tab.push_back({slint::SharedString(name),
                                    slint::SharedString(formatTime_HHMMSS(duration)), // The HH:MM:SS string
                                    (float)duration});
    }

    std::vector<TimeLog_Project> slintVec_TimeLog_Project;
    for (const auto &[name, duration] : translatedTimeLog_Project)
    {
        slintVec_TimeLog_Project.push_back({slint::SharedString(name),
                                            slint::SharedString(formatTime_HHMMSS(duration)), // The HH:MM:SS string
                                            (float)duration});
    }

    // Sort so most used comes at top
    std::sort(slintVec_TimeLog.begin(), slintVec_TimeLog.end(), [](const TimeLog &a, const TimeLog &b)
              { return a.duration_i > b.duration_i; });

    std::sort(slintVec_TimeLog_Tab.begin(), slintVec_TimeLog_Tab.end(), [](const TimeLog_Tab &a, const TimeLog_Tab &b)
              { return a.duration_i > b.duration_i; });

    std::sort(slintVec_TimeLog_Project.begin(), slintVec_TimeLog_Project.end(), [](const TimeLog_Project &a, const TimeLog_Project &b)
              { return a.duration_i > b.duration_i; });

    // Raw Apps extraction from rawTimeLog with Limits & Goals from AppState
    std::vector<AppGoalData> slintVec_RawApps;
    {
        std::map<std::string,int> limits;
        std::map<std::string,int> goals;

        {
            std::lock_guard<std::mutex> lock(AppState::stateMutex);

            limits = AppState::state.appLimits;
            goals = AppState::state.appGoals;
        }
        for (const auto &[raw, duration] : rawTimeLog)
        {
            int limit = 0;
            int goal = 0;

            if (limits.count(raw))
                limit = limits.at(raw);

            if (goals.count(raw))
                goal = goals.at(raw);

            std::string limit_rem =
                LimitsManager::getLimitRemaining(raw, duration, limit);

            std::string goal_rem =
                LimitsManager::getGoalRemaining(raw, duration, goal);

            // get display name using aliasManager under lua lock or default
            std::string displayName;
            {
                // aliasManager uses extensions (lua state), so lock luaMutex as well if it exists or use stateMutex
                std::lock_guard<std::mutex> lock(AppState::stateMutex);
                displayName = aliasManager.getAlias(raw);
            }

            slintVec_RawApps.push_back({
                slint::SharedString(displayName),
                slint::SharedString(raw),
                limit,
                goal,
                slint::SharedString(limit_rem),
                slint::SharedString(goal_rem)
            });
        }
    }




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
    size_t switchHistoryLimit = std::min(tempSwitchVec.size(), size_t(100));

    for (size_t i = 0; i < switchHistoryLimit; ++i)
    {
        const auto &item = tempSwitchVec[i];
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

    std::string theme = AppState::configManager.getConfig<std::string>("theme", "dark");
    bool isDark = (theme != "light");

    slint::ComponentWeakHandle<MainWindow> weak(ui.value());
    slint::invoke_from_event_loop([weak, slintVec_TimeLog, slintVec_TimeLog_Tab, slintVec_TimeLog_Project, slintVec_SwitchHistory, slintVec_RawApps, totalTrackedTime, totalTrackedTime_Tab, totalTrackedTime_Project, currentWindowName, theme, isDark, this]()
    {
        if (auto handle = weak.lock()) {
                bool autostartVal = AppState::configManager.getConfig<bool>("autostart", false);
                bool headlessVal = AppState::configManager.getConfig<bool>("headless-mode", false);
                bool telemetryVal = AppState::configManager.getConfig<bool>("anonymous-telemetry", false);
                bool noTitleBarVal = AppState::configManager.getConfig<bool>("no-title-bar", false);
                float cornerRoundnessVal = AppState::configManager.getConfig<float>("corner-roundness", 0.5f);
                bool showPromptVal = AppState::configManager.isFirstLaunch();
                
                bool killAppsVal = AppState::configManager.getConfig<bool>("kill-apps", true);
                bool hwAccelVal = AppState::configManager.getConfig<bool>("hardware-acceleration", true);
                bool allowCustomVal = AppState::configManager.getConfig<bool>("allow-custom-backends", false);
                bool allowNetworkVal = AppState::configManager.getConfig<bool>("allow-network-activity", true);

                std::string nowPlayingTitleVal;
                std::string nowPlayingUrlVal;
                {
                    std::lock_guard<std::mutex> lock(AppState::stateMutex);
                    nowPlayingTitleVal = AppState::state.nowPlayingTitle;
                    nowPlayingUrlVal = AppState::state.nowPlayingUrl;
                }
                
                if ((*handle)->get_themeMode_S() != slint::SharedString(theme))
                    (*handle)->set_themeMode_S(slint::SharedString(theme));
                if ((*handle)->get_isDarkMode() != isDark)
                    (*handle)->set_isDarkMode(isDark);
                if ((*handle)->get_autostart_S() != autostartVal)
                    (*handle)->set_autostart_S(autostartVal);
                if ((*handle)->get_headless_S() != headlessVal)
                    (*handle)->set_headless_S(headlessVal);
                if ((*handle)->get_telemetry_S() != telemetryVal)
                    (*handle)->set_telemetry_S(telemetryVal);
                if ((*handle)->get_no_title_bar_enabled() != noTitleBarVal)
                    (*handle)->set_no_title_bar_enabled(noTitleBarVal);
                if ((*handle)->get_cornerRoundness_S() != cornerRoundnessVal)
                    (*handle)->set_cornerRoundness_S(cornerRoundnessVal);
                if ((*handle)->get_showTelemetryPrompt_S() != showPromptVal)
                    (*handle)->set_showTelemetryPrompt_S(showPromptVal);
                if ((*handle)->get_windowName_S() != slint::SharedString(currentWindowName))
                    (*handle)->set_windowName_S(slint::SharedString(currentWindowName));

                if ((*handle)->get_trackedTime_S() != (float)totalTrackedTime)
                    (*handle)->set_trackedTime_S((float)totalTrackedTime);
                if ((*handle)->get_trackedTime_Tab_S() != (float)totalTrackedTime_Tab)
                    (*handle)->set_trackedTime_Tab_S((float)totalTrackedTime_Tab);
                if ((*handle)->get_trackedTime_Project_S() != (float)totalTrackedTime_Project)
                    (*handle)->set_trackedTime_Project_S((float)totalTrackedTime_Project);

                if ((*handle)->get_killApps_S() != killAppsVal)
                    (*handle)->set_killApps_S(killAppsVal);
                if ((*handle)->get_hardwareAccel_S() != hwAccelVal)
                    (*handle)->set_hardwareAccel_S(hwAccelVal);
                if ((*handle)->get_allowCustomBackends_S() != allowCustomVal)
                    (*handle)->set_allowCustomBackends_S(allowCustomVal);
                if ((*handle)->get_allowNetworkActivity_S() != allowNetworkVal)
                    (*handle)->set_allowNetworkActivity_S(allowNetworkVal);

                if ((*handle)->get_nowPlayingTitle_S() != slint::SharedString(nowPlayingTitleVal))
                    (*handle)->set_nowPlayingTitle_S(slint::SharedString(nowPlayingTitleVal));
                if ((*handle)->get_nowPlayingUrl_S() != slint::SharedString(nowPlayingUrlVal))
                    (*handle)->set_nowPlayingUrl_S(slint::SharedString(nowPlayingUrlVal));

                // Surgical update to prevent layout panics during resize/maximize
                auto syncModel = [](auto model, const auto& vec) 
                {
                    size_t existing_count = model->row_count();
                    size_t new_count = vec.size();
                    size_t min_count = (std::min)(existing_count, new_count);

                    // Update existing rows only if they changed
                    for (size_t i = 0; i < min_count; ++i) 
                    {
                        if (auto existing = model->row_data(i)) 
                        {
                            if (*existing == vec[i]) 
                            {
                                continue;
                            }
                        }
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
                syncModel(timeLogModelTab, slintVec_TimeLog_Tab);
                syncModel(timeLogModelProject, slintVec_TimeLog_Project);
                syncModel(switchHistoryModel, slintVec_SwitchHistory);

                // Sync rawAppsModel
                auto syncModelRaw = [](auto model, const auto& vec) 
                {
                    size_t existing_count = model->row_count();
                    size_t new_count = vec.size();
                    size_t min_count = (std::min)(existing_count, new_count);

                    for (size_t i = 0; i < min_count; ++i) 
                    {
                        if (auto existing = model->row_data(i)) 
                        {
                            if (*existing == vec[i]) 
                            {
                                continue;
                            }
                        }
                        model->set_row_data(i, vec[i]);
                    }

                    while (model->row_count() > new_count) 
                    {
                        model->erase(model->row_count() - 1);
                    }

                    for (size_t i = existing_count; i < new_count; ++i) 
                    {
                        model->push_back(vec[i]);
                    }
                };
                syncModelRaw(rawAppsModel, slintVec_RawApps);

                // Reconstruct and sync timeline events
                int preset = (*handle)->get_timelinePresetHours();
                int startH = (*handle)->get_timelineStartHour();
                int endH = (*handle)->get_timelineEndHour();

                static int lastPreset = -1;
                static int lastStartH = -1;
                static int lastEndH = -1;
                static auto lastTimelineUpdate = std::chrono::steady_clock::now();

                auto now = std::chrono::steady_clock::now();
                bool valuesChanged = (preset != lastPreset || startH != lastStartH || endH != lastEndH);
                bool timeElapsed = (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTimelineUpdate).count() >= 1000);

                if (valuesChanged || timeElapsed)
                {
                    TimelineManager::updateTimeline(preset, startH, endH);
                    lastPreset = preset;
                    lastStartH = startH;
                    lastEndH = endH;
                    lastTimelineUpdate = now;
                }

                auto getAppColor = [](const std::string& appName) -> slint::Color {
                    float h = static_cast<float>((appName.length() * 47) % 360);
                    return slint::Color::from_hsva(h, 0.75f, 0.85f, 1.0f);
                };

                std::vector<TimelineBlock> slintVec_Timeline;
                std::vector<TimelineMarker> slintVec_TimelineMarkers;
                {
                    std::lock_guard<std::mutex> lock(AppState::timelineMutex);
                    for (const auto& item : AppState::timelineEvents)
                    {
                        slintVec_Timeline.push_back(TimelineBlock{
                            static_cast<float>(item.x),
                            static_cast<float>(item.width),
                            getAppColor(item.appName),
                            slint::SharedString(item.appName),
                            slint::SharedString(item.duration),
                            slint::SharedString(item.timeRange)
                        });
                    }
                    for (const auto& m : AppState::timelineMarkers)
                    {
                        slintVec_TimelineMarkers.push_back(TimelineMarker{
                            static_cast<float>(m.x),
                            slint::SharedString(m.label)
                        });
                    }
                }
                syncModel(timelineBlocksModel, slintVec_Timeline);
                syncModel(timelineMarkersModel, slintVec_TimelineMarkers);
            }
    });

}

void UiModelManager::update_Interpreted(const std::map<std::string, uint64_t> &rawTimeLog,
                            const std::map<std::string, uint64_t> &rawTimeLog_Tab,
                            const std::map<std::string, uint64_t> &rawTimeLog_Project,
                            const std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> &rawHistory,
                            std::string &currentWindowName,
                            uint64_t &totalTrackedTime,
                            uint64_t &totalTrackedTime_Tab,
                            uint64_t &totalTrackedTime_Project,
                            AliasManager &aliasManager)
{
    currentWindowName = aliasManager.getAlias(currentWindowName);

    //----------------------TIME LOG-----------------------------------------------

    std::map<std::string, uint64_t> translatedTimeLog;
    std::map<std::string, uint64_t> translatedTimeLog_Tab;
    std::map<std::string, uint64_t> translatedTimeLog_Project;

    for (const auto &[raw, duration] : rawTimeLog)
    {
        translatedTimeLog[aliasManager.getAlias(raw)] += duration;
        totalTrackedTime += duration;
    }

    bool isTabView;
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        isTabView= AppState::state.useTabView;
    }

    //if tab view is on, we show raw data without aliasing, because it is per *tab* and not per site, so aliasing would just mess things up. If tab view is off, we alias as normal
    if (isTabView)
    {
        for (const auto &[raw, duration] : rawTimeLog_Tab)
        {
            translatedTimeLog_Tab[raw] += duration;
            totalTrackedTime_Tab += duration;
        }
    }
    else
    {
        for (const auto &[raw, duration] : rawTimeLog_Tab)
        {
            translatedTimeLog_Tab[aliasManager.getAlias_Tab(raw)] += duration;
            totalTrackedTime_Tab += duration;
        }
    }

    //Projects
    bool isRawView;
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        isRawView= AppState::state.isRawProjectView;
    }

    //if its raw view, just raw dog it
    if (isRawView)
    {
        for (const auto &[raw, duration] : rawTimeLog_Project)
        {
            translatedTimeLog_Project[raw] += duration;
            totalTrackedTime_Project += duration;
        }
    }
    //Parse it first and then find an aliases if present
    else 
    {
        //How does this parse? interesting question
        //Lets take an example: test.cpp - HPR - Visual Studio Code is the window title
        //what it will do is first check if it ends with " - Visual Studio Code", if it does, it will remove that part, so now we have "test.cpp - HPR"
        //Then it will find the last occurrence of " - ", which is the separator between project
        //keep in mind it finds " - " and not just "-", because project names can have dashes in them, but the separator is always " - "
        //no person uses " - " in their project names, that would be shit, so this is a safe separator to use"
        for (const auto &[raw, duration] : rawTimeLog_Project)
        {
            std::string middleMan;

            const std::string suffix = " - Visual Studio Code";
            std::string stripped = raw;
            
            if (stripped.size() >= suffix.size() && 
                stripped.substr(stripped.size() - suffix.size()) == suffix)
                stripped = stripped.substr(0, stripped.size() - suffix.size());
            
            // rfind last " - "
            size_t pos = stripped.rfind(" - ");
            if (pos == std::string::npos)
                middleMan = stripped; // no separator, return raw
            else
                middleMan = stripped.substr(pos + 3);

            translatedTimeLog_Project[aliasManager.getAlias_Project(middleMan)] += duration;
            totalTrackedTime_Project += duration;
        }
    }

    // interpreter uses Value, not typed structs
    std::vector<slint::interpreter::Value> slintVec_TimeLog;
    std::vector<slint::interpreter::Value> slintVec_TimeLog_Tab;
    std::vector<slint::interpreter::Value> slintVec_TimeLog_Project;

    for (const auto &[name, duration] : translatedTimeLog)
    {
        slint::interpreter::Struct entry;
        entry.set_field("name",       slint::interpreter::Value(slint::SharedString(name)));
        entry.set_field("duration",   slint::interpreter::Value(slint::SharedString(formatTime_HHMMSS(duration))));
        entry.set_field("duration_i", slint::interpreter::Value((double)duration));
        slintVec_TimeLog.push_back(slint::interpreter::Value(entry));
    }

    for (const auto &[name, duration] : translatedTimeLog_Tab)
    {
        slint::interpreter::Struct entry;
        entry.set_field("name",       slint::interpreter::Value(slint::SharedString(name)));
        entry.set_field("duration",   slint::interpreter::Value(slint::SharedString(formatTime_HHMMSS(duration))));
        entry.set_field("duration_i", slint::interpreter::Value((double)duration));
        slintVec_TimeLog_Tab.push_back(slint::interpreter::Value(entry));
    }

    for (const auto &[name, duration] : translatedTimeLog_Project)
    {
        slint::interpreter::Struct entry;
        entry.set_field("name",       slint::interpreter::Value(slint::SharedString(name)));
        entry.set_field("duration",   slint::interpreter::Value(slint::SharedString(formatTime_HHMMSS(duration))));
        entry.set_field("duration_i", slint::interpreter::Value((double)duration));
        slintVec_TimeLog_Project.push_back(slint::interpreter::Value(entry));
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

    std::sort(slintVec_TimeLog_Tab.begin(), slintVec_TimeLog_Tab.end(),
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

    std::sort(slintVec_TimeLog_Project.begin(), slintVec_TimeLog_Project.end(),
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

    // Raw Apps extraction for interpreted mode with Limits & Goals from AppState
    std::vector<slint::interpreter::Value> slintVec_RawApps;
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        for (const auto &[raw, duration] : rawTimeLog)
        {
            int limit = 0;
            int goal = 0;
            if (AppState::state.appLimits.count(raw)) limit = AppState::state.appLimits.at(raw);
            if (AppState::state.appGoals.count(raw)) goal = AppState::state.appGoals.at(raw);

            std::string limit_rem = LimitsManager::getLimitRemaining(raw, duration, limit);
            std::string goal_rem = LimitsManager::getGoalRemaining(raw, duration, goal);

            // get display name using aliasManager under lua/state mutex lock (already locked via stateMutex)
            std::string displayName = aliasManager.getAlias(raw);

            slint::interpreter::Struct entry;
            entry.set_field("name", slint::interpreter::Value(slint::SharedString(displayName)));
            entry.set_field("raw_name", slint::interpreter::Value(slint::SharedString(raw)));
            entry.set_field("limit_mins", slint::interpreter::Value((double)limit));
            entry.set_field("goal_mins", slint::interpreter::Value((double)goal));
            entry.set_field("limit_remaining", slint::interpreter::Value(slint::SharedString(limit_rem)));
            entry.set_field("goal_remaining", slint::interpreter::Value(slint::SharedString(goal_rem)));
            slintVec_RawApps.push_back(slint::interpreter::Value(entry));
        }
    }



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
    size_t switchHistoryLimit = std::min(tempSwitchVec.size(), size_t(100));

    for (size_t i = 0; i < switchHistoryLimit; ++i)
    {
        const auto &item = tempSwitchVec[i];
        slint::interpreter::Struct entry;
        entry.set_field("fromWindow",      slint::interpreter::Value(slint::SharedString(aliasManager.getAlias(item.from))));
        entry.set_field("toWindow",        slint::interpreter::Value(slint::SharedString(aliasManager.getAlias(item.to))));
        entry.set_field("maxTimeStamp", slint::interpreter::Value(slint::SharedString(convertToTime_HHMMSS_12(item.maxVal))));
        slintVec_SwitchHistory.push_back(slint::interpreter::Value(entry));
    }

    //-----------------Actually pushing changes to slint for rendering-----------------------

    if (!ui_interp.has_value()) 
    {
        return;
    }

    std::string theme = AppState::configManager.getConfig<std::string>("theme", "dark");
    bool isDark = (theme != "light");

    slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> weak(ui_interp.value());
    slint::invoke_from_event_loop(
        [weak,
        slintVec_TimeLog,
        slintVec_TimeLog_Tab,
        slintVec_TimeLog_Project,
        slintVec_SwitchHistory,
        slintVec_RawApps,
        totalTrackedTime,
        totalTrackedTime_Tab,
        totalTrackedTime_Project,
        currentWindowName,
        theme,
        isDark,
        this]()
    {
        if (auto handle = weak.lock())
        {
            bool autostartVal = AppState::configManager.getConfig<bool>("autostart", false);
            bool headlessVal = AppState::configManager.getConfig<bool>("headless-mode", false);
            bool telemetryVal = AppState::configManager.getConfig<bool>("anonymous-telemetry", false);
            bool noTitleBarVal = AppState::configManager.getConfig<bool>("no-title-bar", false);
            float cornerRoundnessVal = AppState::configManager.getConfig<float>("corner-roundness", 0.5f);
            bool showPromptVal = AppState::configManager.isFirstLaunch();

            bool killAppsVal = AppState::configManager.getConfig<bool>("kill-apps", true);
            bool hwAccelVal = AppState::configManager.getConfig<bool>("hardware-acceleration", true);
            bool allowCustomVal = AppState::configManager.getConfig<bool>("allow-custom-backends", false);
            bool allowNetworkVal = AppState::configManager.getConfig<bool>("allow-network-activity", true);

            std::string nowPlayingTitleVal;
            std::string nowPlayingUrlVal;
            {
                std::lock_guard<std::mutex> lock(AppState::stateMutex);
                nowPlayingTitleVal = AppState::state.nowPlayingTitle;
                nowPlayingUrlVal = AppState::state.nowPlayingUrl;
            }

            auto getProp = [&](const char* name) {
                return (*handle)->get_property(name);
            };

            auto setPropIfChanged = [&](const char* name, const slint::interpreter::Value& val) {
                if (auto current = getProp(name)) {
                    if (*current == val) {
                        return;
                    }
                }
                (*handle)->set_property(name, val);
            };

            setPropIfChanged("themeMode_S", slint::interpreter::Value(slint::SharedString(theme)));
            setPropIfChanged("isDarkMode", slint::interpreter::Value(isDark));
            setPropIfChanged("autostart_S", slint::interpreter::Value(autostartVal));
            setPropIfChanged("headless_S", slint::interpreter::Value(headlessVal));
            setPropIfChanged("telemetry_S", slint::interpreter::Value(telemetryVal));
            setPropIfChanged("no_title_bar_enabled", slint::interpreter::Value(noTitleBarVal));
            setPropIfChanged("cornerRoundness_S", slint::interpreter::Value((double)cornerRoundnessVal));
            setPropIfChanged("showTelemetryPrompt_S", slint::interpreter::Value(showPromptVal));
            setPropIfChanged("windowName_S", slint::interpreter::Value(slint::SharedString(currentWindowName)));
            setPropIfChanged("trackedTime_S", slint::interpreter::Value((double)totalTrackedTime));
            setPropIfChanged("trackedTime_Tab_S", slint::interpreter::Value((double)totalTrackedTime_Tab));
            setPropIfChanged("trackedTime_Project_S", slint::interpreter::Value((double)totalTrackedTime_Project));

            setPropIfChanged("killApps_S", slint::interpreter::Value(killAppsVal));
            setPropIfChanged("hardwareAccel_S", slint::interpreter::Value(hwAccelVal));
            setPropIfChanged("allowCustomBackends_S", slint::interpreter::Value(allowCustomVal));
            setPropIfChanged("allowNetworkActivity_S", slint::interpreter::Value(allowNetworkVal));

            setPropIfChanged("nowPlayingTitle_S", slint::interpreter::Value(slint::SharedString(nowPlayingTitleVal)));
            setPropIfChanged("nowPlayingUrl_S", slint::interpreter::Value(slint::SharedString(nowPlayingUrlVal)));

            auto syncModel = [](
                std::shared_ptr<slint::VectorModel<slint::interpreter::Value>> model,
                const std::vector<slint::interpreter::Value>& vec)
            {
                size_t existing_count = model->row_count();
                size_t new_count = vec.size();
                size_t min_count = (std::min)(existing_count, new_count);

                // Update existing rows only if they changed
                for (size_t i = 0; i < min_count; ++i)
                {
                    if (auto existing = model->row_data(i)) 
                    {
                        if (*existing == vec[i]) 
                        {
                            continue;
                        }
                    }
                    model->set_row_data(i, vec[i]);
                }

                // Remove excess rows
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

            syncModel(timeLogModel_interp,        slintVec_TimeLog);
            syncModel(timeLogModelTab_interp,     slintVec_TimeLog_Tab);
            syncModel(timeLogModelProject_interp, slintVec_TimeLog_Project);
            syncModel(switchHistoryModel_interp,  slintVec_SwitchHistory);
            syncModel(rawAppsModel_interp,        slintVec_RawApps);

            // Reconstruct and sync timeline events (interpreted)
            int preset = (*handle)->get_property("timelinePresetHours").value_or(slint::interpreter::Value(24.0)).to_number().value_or(24.0);
            int startH = (*handle)->get_property("timelineStartHour").value_or(slint::interpreter::Value(0.0)).to_number().value_or(0.0);
            int endH = (*handle)->get_property("timelineEndHour").value_or(slint::interpreter::Value(24.0)).to_number().value_or(24.0);

            static int lastPreset = -1;
            static int lastStartH = -1;
            static int lastEndH = -1;
            static auto lastTimelineUpdate = std::chrono::steady_clock::now();

            auto now = std::chrono::steady_clock::now();
            bool valuesChanged = (preset != lastPreset || startH != lastStartH || endH != lastEndH);
            bool timeElapsed = (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTimelineUpdate).count() >= 1000);

            if (valuesChanged || timeElapsed)
            {
                TimelineManager::updateTimeline(preset, startH, endH);
                lastPreset = preset;
                lastStartH = startH;
                lastEndH = endH;
                lastTimelineUpdate = now;
            }

            auto getAppColor = [](const std::string& appName) -> slint::Color {
                float h = static_cast<float>((appName.length() * 47) % 360);
                return slint::Color::from_hsva(h, 0.75f, 0.85f, 1.0f);
            };

            std::vector<slint::interpreter::Value> slintVec_Timeline;
            std::vector<slint::interpreter::Value> slintVec_TimelineMarkers;
            {
                std::lock_guard<std::mutex> lock(AppState::timelineMutex);
                for (const auto& item : AppState::timelineEvents)
                {
                    slint::interpreter::Struct entry;
                    entry.set_field("x", slint::interpreter::Value(item.x));
                    entry.set_field("width", slint::interpreter::Value(item.width));
                    entry.set_field("color", slint::interpreter::Value(getAppColor(item.appName)));
                    entry.set_field("appName", slint::interpreter::Value(slint::SharedString(item.appName)));
                    entry.set_field("duration", slint::interpreter::Value(slint::SharedString(item.duration)));
                    entry.set_field("timeRange", slint::interpreter::Value(slint::SharedString(item.timeRange)));
                    slintVec_Timeline.push_back(slint::interpreter::Value(entry));
                }
                for (const auto& m : AppState::timelineMarkers)
                {
                    slint::interpreter::Struct entry;
                    entry.set_field("x", slint::interpreter::Value(m.x));
                    entry.set_field("label", slint::interpreter::Value(slint::SharedString(m.label)));
                    slintVec_TimelineMarkers.push_back(slint::interpreter::Value(entry));
                }
            }
            syncModel(timelineBlocksModel_interp, slintVec_Timeline);
            syncModel(timelineMarkersModel_interp, slintVec_TimelineMarkers);
        }
    });

}

void UiModelManager::showInsights(const std::string mostUsed,
                          const std::string totalTrackedTime,
                          const std::string switchCount,
                          const std::string mostSwitchedFrom,
                          const std::string mostSwitchedTo,
                          const std::string mostFocusedSession,
                          const std::string mostProductiveHour,
                          const std::string escapePattern,
                          const std::string returnRate,
                          const std::string avgFocusSession,
                          const std::string mostDistractedDay,
                          const std::string productiveDays,
                          const std::string screenTimeVsAvg,
                          const std::string focusDipHour,
                          const std::string deepWorkBeforeNoon,
                          const std::string weekendVsWeekday)
{
    if (!ui.has_value()) {
        return;
    }
    slint::ComponentWeakHandle<MainWindow> weak(ui.value());
    slint::invoke_from_event_loop([weak, mostUsed, totalTrackedTime, switchCount, mostSwitchedFrom, mostSwitchedTo, mostFocusedSession, mostProductiveHour,
                                   escapePattern, returnRate, avgFocusSession, mostDistractedDay, productiveDays, screenTimeVsAvg, focusDipHour, deepWorkBeforeNoon, weekendVsWeekday, this]()
    {
        if (auto handle = weak.lock())
        {
            if ((*handle)->get_mostUsedApp_S() != slint::SharedString(mostUsed)) (*handle)->set_mostUsedApp_S(slint::SharedString(mostUsed));
            if ((*handle)->get_totalTrackedTime_S() != slint::SharedString(totalTrackedTime)) (*handle)->set_totalTrackedTime_S(slint::SharedString(totalTrackedTime));
            if ((*handle)->get_totalSwitches_S() != slint::SharedString(switchCount)) (*handle)->set_totalSwitches_S(slint::SharedString(switchCount));
            if ((*handle)->get_mostSwitchedFrom_S() != slint::SharedString(mostSwitchedFrom)) (*handle)->set_mostSwitchedFrom_S(slint::SharedString(mostSwitchedFrom));
            if ((*handle)->get_mostSwitchedTo_S() != slint::SharedString(mostSwitchedTo)) (*handle)->set_mostSwitchedTo_S(slint::SharedString(mostSwitchedTo));
            if ((*handle)->get_longestFocus_S() != slint::SharedString(mostFocusedSession)) (*handle)->set_longestFocus_S(slint::SharedString(mostFocusedSession));
            if ((*handle)->get_peakHour_S() != slint::SharedString(mostProductiveHour)) (*handle)->set_peakHour_S(slint::SharedString(mostProductiveHour));
            if ((*handle)->get_escapePattern_S() != slint::SharedString(escapePattern)) (*handle)->set_escapePattern_S(slint::SharedString(escapePattern));
            if ((*handle)->get_returnRate_S() != slint::SharedString(returnRate)) (*handle)->set_returnRate_S(slint::SharedString(returnRate));
            if ((*handle)->get_avgFocusSession_S() != slint::SharedString(avgFocusSession)) (*handle)->set_avgFocusSession_S(slint::SharedString(avgFocusSession));
            if ((*handle)->get_mostDistractedDay_S() != slint::SharedString(mostDistractedDay)) (*handle)->set_mostDistractedDay_S(slint::SharedString(mostDistractedDay));
            if ((*handle)->get_productiveDays_S() != slint::SharedString(productiveDays)) (*handle)->set_productiveDays_S(slint::SharedString(productiveDays));
            if ((*handle)->get_screenTimeVsAvg_S() != slint::SharedString(screenTimeVsAvg)) (*handle)->set_screenTimeVsAvg_S(slint::SharedString(screenTimeVsAvg));
            if ((*handle)->get_focusDipHour_S() != slint::SharedString(focusDipHour)) (*handle)->set_focusDipHour_S(slint::SharedString(focusDipHour));
            if ((*handle)->get_deepWorkBeforeNoon_S() != slint::SharedString(deepWorkBeforeNoon)) (*handle)->set_deepWorkBeforeNoon_S(slint::SharedString(deepWorkBeforeNoon));
            if ((*handle)->get_weekendVsWeekday_S() != slint::SharedString(weekendVsWeekday)) (*handle)->set_weekendVsWeekday_S(slint::SharedString(weekendVsWeekday));
        }
    });
}

void UiModelManager::showInsights_Interpreted(const std::string mostUsed,
                          const std::string totalTrackedTime,
                          const std::string switchCount,
                          const std::string mostSwitchedFrom,
                          const std::string mostSwitchedTo,
                          const std::string mostFocusedSession,
                          const std::string mostProductiveHour,
                          const std::string escapePattern,
                          const std::string returnRate,
                          const std::string avgFocusSession,
                          const std::string mostDistractedDay,
                          const std::string productiveDays,
                          const std::string screenTimeVsAvg,
                          const std::string focusDipHour,
                          const std::string deepWorkBeforeNoon,
                          const std::string weekendVsWeekday)
{
    slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> weak(ui_interp.value());
    slint::invoke_from_event_loop([weak, mostUsed, totalTrackedTime, switchCount, mostSwitchedFrom, mostSwitchedTo, mostFocusedSession, mostProductiveHour,
                                   escapePattern, returnRate, avgFocusSession, mostDistractedDay, productiveDays, screenTimeVsAvg, focusDipHour, deepWorkBeforeNoon, weekendVsWeekday, this]()
    {
        if (auto handle = weak.lock())
        {
            auto getProp = [&](const char* name) {
                return (*handle)->get_property(name);
            };

            auto setPropIfChanged = [&](const char* name, const slint::interpreter::Value& val) {
                if (auto current = getProp(name)) {
                    if (*current == val) {
                        return;
                    }
                }
                (*handle)->set_property(name, val);
            };

            setPropIfChanged("mostUsedApp_S",        slint::interpreter::Value(slint::SharedString(mostUsed)));
            setPropIfChanged("totalTrackedTime_S",   slint::interpreter::Value(slint::SharedString(totalTrackedTime)));
            setPropIfChanged("totalSwitches_S",      slint::interpreter::Value(slint::SharedString(switchCount)));
            setPropIfChanged("mostSwitchedFrom_S",   slint::interpreter::Value(slint::SharedString(mostSwitchedFrom)));
            setPropIfChanged("mostSwitchedTo_S",     slint::interpreter::Value(slint::SharedString(mostSwitchedTo)));
            setPropIfChanged("longestFocus_S",       slint::interpreter::Value(slint::SharedString(mostFocusedSession)));
            setPropIfChanged("peakHour_S",           slint::interpreter::Value(slint::SharedString(mostProductiveHour)));
            setPropIfChanged("escapePattern_S",      slint::interpreter::Value(slint::SharedString(escapePattern)));
            setPropIfChanged("returnRate_S",         slint::interpreter::Value(slint::SharedString(returnRate)));
            setPropIfChanged("avgFocusSession_S",    slint::interpreter::Value(slint::SharedString(avgFocusSession)));
            setPropIfChanged("mostDistractedDay_S",  slint::interpreter::Value(slint::SharedString(mostDistractedDay)));
            setPropIfChanged("productiveDays_S",     slint::interpreter::Value(slint::SharedString(productiveDays)));
            setPropIfChanged("screenTimeVsAvg_S",    slint::interpreter::Value(slint::SharedString(screenTimeVsAvg)));
            setPropIfChanged("focusDipHour_S",       slint::interpreter::Value(slint::SharedString(focusDipHour)));
            setPropIfChanged("deepWorkBeforeNoon_S", slint::interpreter::Value(slint::SharedString(deepWorkBeforeNoon)));
            setPropIfChanged("weekendVsWeekday_S",   slint::interpreter::Value(slint::SharedString(weekendVsWeekday)));
        }
    });
}

void UiModelManager::showExtensions(const std::vector<std::pair<std::string,std::string>>& extensions)
{
    if (extensions == lastKnownExtensions) return;
    lastKnownExtensions = extensions;

    if (!ui.has_value()) return;
    std::vector<LoadedExtension_S> vec;
    for (const auto& [author, name] : extensions)
        vec.push_back({ slint::SharedString(author), slint::SharedString(name) });

    slint::ComponentWeakHandle<MainWindow> weak(ui.value());
    slint::invoke_from_event_loop([weak, vec, this]()
    {
        if (auto handle = weak.lock())
        {
            extensionsModel->clear();
            for (const auto& e : vec)
                extensionsModel->push_back(e);
        }
    });
}

void UiModelManager::showExtensions_Interpreted(const std::vector<std::pair<std::string,std::string>>& extensions)
{
    if (extensions == lastKnownExtensions) return;
    lastKnownExtensions = extensions;

    std::vector<slint::interpreter::Value> vec;
    for (const auto& [author, name] : extensions)
    {
        slint::interpreter::Struct entry;
        entry.set_field("author-name",     slint::interpreter::Value(slint::SharedString(author)));
        entry.set_field("extension-name",  slint::interpreter::Value(slint::SharedString(name)));
        vec.push_back(slint::interpreter::Value(entry));
    }

    slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> weak(ui_interp.value());
    slint::invoke_from_event_loop([weak, vec, this]()
    {
        if (auto handle = weak.lock())
        {
            extensionsModel_interp->clear();
            for (const auto& e : vec)
                extensionsModel_interp->push_back(e);
        }
    });
}

void UiModelManager::showFunStats(const std::string& cpu, const std::string& ram, const std::string& ext, const std::string& threads)
{
    if (!ui.has_value()) return;
    slint::ComponentWeakHandle<MainWindow> weak(ui.value());
    slint::invoke_from_event_loop([weak, cpu, ram, ext, threads]()
    {
        if (auto handle = weak.lock())
        {
            if ((*handle)->get_cpuUsage() != slint::SharedString(cpu)) (*handle)->set_cpuUsage(slint::SharedString(cpu));
            if ((*handle)->get_ramUsage() != slint::SharedString(ram)) (*handle)->set_ramUsage(slint::SharedString(ram));
            if ((*handle)->get_loadedExtensions() != slint::SharedString(ext)) (*handle)->set_loadedExtensions(slint::SharedString(ext));
            if ((*handle)->get_activeThreads() != slint::SharedString(threads)) (*handle)->set_activeThreads(slint::SharedString(threads));
        }
    });
}

void UiModelManager::showFunStats_Interpreted(const std::string& cpu, const std::string& ram, const std::string& ext, const std::string& threads)
{
    if (!ui_interp.has_value()) return;
    slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> weak(ui_interp.value());
    slint::invoke_from_event_loop([weak, cpu, ram, ext, threads]()
    {
        if (auto handle = weak.lock())
        {
            auto getProp = [&](const char* name) {
                return (*handle)->get_property(name);
            };

            auto setPropIfChanged = [&](const char* name, const slint::interpreter::Value& val) {
                if (auto current = getProp(name)) {
                    if (*current == val) {
                        return;
                    }
                }
                (*handle)->set_property(name, val);
            };

            setPropIfChanged("cpuUsage", slint::interpreter::Value(slint::SharedString(cpu)));
            setPropIfChanged("ramUsage", slint::interpreter::Value(slint::SharedString(ram)));
            setPropIfChanged("loadedExtensions", slint::interpreter::Value(slint::SharedString(ext)));
            setPropIfChanged("activeThreads", slint::interpreter::Value(slint::SharedString(threads)));
        }
    });
}