// HPR.hpp
#pragma once

#include "appState.hpp"
#include "aliasManager.hpp"

//Slint stuff
#include "app-window.h"
#include <slint.h>

#include <thread>
#include <atomic>
#include <optional>

class HPR {
    public:
        HPR();
        ~HPR();
        void run(); // blocking call i believe

    private:
        void trackingLoop(); // runs on separate thread so that it polls shit continously (correct spelling?)

    private:
        std::optional<slint::ComponentHandle<MainWindow>> ui;
        std::atomic<bool> running{true};
        std::thread tracker;

        AliasManager aliasManager;
};