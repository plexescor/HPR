#pragma once
#include "app-window.h"
#include "extensionManager.hpp"
#include <slint.h>
#include <slint-interpreter.h>
class UiEventBridge
{
    public:
       // Compiled UI Constructor
        UiEventBridge(slint::ComponentHandle<MainWindow>& ui, ExtensionManager* extMgr = nullptr, HPRInterpreter* interpreter = nullptr);

        // Interpreted UI Constructor
        UiEventBridge(slint::ComponentHandle<slint::interpreter::ComponentInstance>& ui,  ExtensionManager* extMgr = nullptr, HPRInterpreter* interpreter = nullptr);

        ~UiEventBridge();

    private:
        void init();
        void showHistoricalDataSingular();
        void showLiveData();
        void tabViewClicked();
        void siteViewClicked();
        void filterViewClicked();
        void rawViewClicked();

    private:
        ExtensionManager* extManager = nullptr;
        HPRInterpreter* interpreter = nullptr;
        size_t loadDbSingularId;
};