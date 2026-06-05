#pragma once
#include <map>

#include "aliasManager.hpp"

//Slint stuff
#include "app-window.h"
#include <slint.h>
#include <vector>
#include <string>
#include <slint-interpreter.h>
class UiModelManager
{
    public: 
        UiModelManager(slint::ComponentHandle<MainWindow>& ui);
        UiModelManager(slint::ComponentHandle<slint::interpreter::ComponentInstance> &ui_handle);
        ~UiModelManager();

        void update(const std::map<std::string, uint64_t> &rawTimeLog,
                            const std::map<std::string, uint64_t> &rawTimeLog_Tab,
                            const std::map<std::string, uint64_t> &rawTimeLog_Project,
                            const std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> &rawHistory,
                            std::string &currentWindowName,
                            uint64_t &totalTrackedTime,
                            uint64_t &totalTrackedTime_Tab,
                            uint64_t &totalTrackedTime_Project,
                            AliasManager &aliasManager
        );

        void update_Interpreted(const std::map<std::string, uint64_t> &rawTimeLog,
                            const std::map<std::string, uint64_t> &rawTimeLog_Tab,
                            const std::map<std::string, uint64_t> &rawTimeLog_Project,
                            const std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> &rawHistory,
                            std::string &currentWindowName,
                            uint64_t &totalTrackedTime,
                            uint64_t &totalTrackedTime_Tab,
                            uint64_t &totalTrackedTime_Project,
                            AliasManager &aliasManager);

        void showInsights(const std::string mostUsed,
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
                          const std::string weekendVsWeekday);

        void showInsights_Interpreted(const std::string mostUsed,
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
                          const std::string weekendVsWeekday);

        void showExtensions(const std::vector<std::pair<std::string,std::string>>& extensions);
        void showExtensions_Interpreted(const std::vector<std::pair<std::string,std::string>>& extensions);
        
        void showFunStats(const std::string& cpu, const std::string& ram, const std::string& ext, const std::string& threads);
        void showFunStats_Interpreted(const std::string& cpu, const std::string& ram, const std::string& ext, const std::string& threads);
    
    private: 
        std::optional<slint::ComponentHandle<MainWindow>> ui;
        std::vector<std::pair<std::string,std::string>> lastKnownExtensions;

        //Slint models
        std::shared_ptr<slint::VectorModel<TimeLog>> timeLogModel;
        std::shared_ptr<slint::VectorModel<TimeLog_Tab>> timeLogModelTab;
        std::shared_ptr<slint::VectorModel<TimeLog_Project>> timeLogModelProject;
        std::shared_ptr<slint::VectorModel<SwitchHistory>> switchHistoryModel;
        std::shared_ptr<slint::VectorModel<LoadedExtension_S>> extensionsModel;
        std::shared_ptr<slint::VectorModel<AppGoalData>> rawAppsModel;
        std::shared_ptr<slint::VectorModel<TimelineEvent>> timelineEventsModel;

        // Middle men
        // std::map<std::string, long> translatedTimeLog;
        // std::map<std::string, long> translatedTimeLog_Tab;

        // interpreter-specific members
        std::optional<slint::ComponentHandle<slint::interpreter::ComponentInstance>> ui_interp;
        std::shared_ptr<slint::VectorModel<slint::interpreter::Value>> timeLogModel_interp;
        std::shared_ptr<slint::VectorModel<slint::interpreter::Value>> timeLogModelTab_interp;
        std::shared_ptr<slint::VectorModel<slint::interpreter::Value>> timeLogModelProject_interp;
        std::shared_ptr<slint::VectorModel<slint::interpreter::Value>> switchHistoryModel_interp;
        std::shared_ptr<slint::VectorModel<slint::interpreter::Value>> extensionsModel_interp;
        std::shared_ptr<slint::VectorModel<slint::interpreter::Value>> rawAppsModel_interp;
        std::shared_ptr<slint::VectorModel<slint::interpreter::Value>> timelineEventsModel_interp;

};  