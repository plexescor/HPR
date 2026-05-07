// HPR.hpp
#pragma once

#include "appState.hpp"
#include "aliasManager.hpp"
#include "uiModelManager.hpp"

//Slint stuff
#include <slint-interpreter.h>

#include <thread>
#include <atomic>
#include <optional>

class HPRInterpreter {
    public:
        HPRInterpreter();
        ~HPRInterpreter();
        void show();
        void hide();
        void run();
        void quit();

    private:
        void trackingLoop(); // runs on separate thread so that it polls shit continously (correct spelling?)
        bool initialiseSlintUiPath();

    private:
        //Interpreter stuff
        slint::interpreter::ComponentCompiler compiler;
        std::optional<slint::interpreter::ComponentDefinition> definition;
        std::optional<slint::ComponentHandle<slint::interpreter::ComponentInstance>> instance;
        std::optional<slint::ComponentWeakHandle<slint::interpreter::ComponentInstance>> weak_instance;

        std::string filePath;
        std::string fileName = "app-window.slint";

        std::atomic<bool> running{true};
        std::thread tracker;

        AliasManager aliasManager;
        std::optional<UiModelManager> modelManager;
};