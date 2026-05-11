#pragma once
#include <map>

#include "aliasManager.hpp"

//Slint stuff
#include "app-window.h"
#include <slint.h>
#include <slint-interpreter.h>
class UiModelManager
{
    public: 
        UiModelManager(slint::ComponentHandle<MainWindow>& ui);
        UiModelManager(slint::ComponentHandle<slint::interpreter::ComponentInstance> &ui_handle);
        ~UiModelManager();

        void update(const std::map<std::string, long> &rawTimeLog,
                            const std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> &rawHistory,
                            std::string &currentWindowName,
                            long &totalTrackedTime,
                            AliasManager &aliasManager
        );

        void update_Interpreted(const std::map<std::string, long> &rawTimeLog,
                            const std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> &rawHistory,
                            std::string &currentWindowName,
                            long &totalTrackedTime,
                            AliasManager &aliasManager);
    
    private: 
        std::optional<slint::ComponentHandle<MainWindow>> ui;

        //Slint models
        std::shared_ptr<slint::VectorModel<TimeLog>> timeLogModel;
        std::shared_ptr<slint::VectorModel<SwitchHistory>> switchHistoryModel;

        //Middle men
        std::map<std::string, long> translatedTimeLog;

        // interpreter-specific members
        std::optional<slint::ComponentHandle<slint::interpreter::ComponentInstance>> ui_interp;
        std::shared_ptr<slint::VectorModel<slint::interpreter::Value>> timeLogModel_interp;
        std::shared_ptr<slint::VectorModel<slint::interpreter::Value>> switchHistoryModel_interp;
};  