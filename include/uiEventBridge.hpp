#pragma once
#include "app-window.h"
#include <slint.h>
#include <slint-interpreter.h>
class UiEventBridge
{
    public:
       // Compiled UI Constructor
        UiEventBridge(slint::ComponentHandle<MainWindow>& ui);

        // Interpreted UI Constructor
        UiEventBridge(slint::ComponentHandle<slint::interpreter::ComponentInstance>& ui);

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
        size_t loadDbSingularId;
};