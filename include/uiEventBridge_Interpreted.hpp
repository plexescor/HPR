#pragma once
#include <slint-interpreter.h>

class UiEventBridge_Interpreted
{
    public:
        UiEventBridge_Interpreted(slint::ComponentHandle<slint::interpreter::ComponentInstance>& ui);
        ~UiEventBridge_Interpreted();

    private:
        void showHistoricalDataSingular();
        void showLiveData();

    private:
        size_t loadDbSingularId;
};