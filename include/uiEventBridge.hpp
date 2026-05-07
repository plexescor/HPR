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

    private:
        size_t loadDbSingularId;
};