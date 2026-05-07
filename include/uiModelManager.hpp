#pragma once
#include <map>

#include "aliasManager.hpp"

//Slint stuff
#include "app-window.h"
#include <slint.h>

class UiModelManager
{
    public: 
        UiModelManager(slint::ComponentHandle<MainWindow>& ui);
        ~UiModelManager();

        void update(const std::map<std::string, long> &rawTimeLog,
                            const std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> &rawHistory,
                            const std::string &currentWindowName,
                            long &totalTrackedTime,
                            AliasManager &aliasManager
        );
    
    private: 
        slint::ComponentHandle<MainWindow> ui;

        //Slint models
        std::shared_ptr<slint::VectorModel<TimeLog>> timeLogModel;
        std::shared_ptr<slint::VectorModel<SwitchHistory>> switchHistoryModel;

        //Middle men
        std::map<std::string, long> translatedTimeLog;
};  