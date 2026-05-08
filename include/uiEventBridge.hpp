#pragma once
#include "app-window.h"
#include <slint.h>

class UiEventBridge
{
    public:
        UiEventBridge(slint::ComponentHandle<MainWindow>& ui);
        ~UiEventBridge();

    private:
        void showHistoricalDataSingular();
        void showLiveData();

    private:
        size_t loadDbSingularId;
};