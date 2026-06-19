// HPR.hpp
#pragma once

#include "appState.hpp"
#include "uiModelManager.hpp"
#include "extensionManager.hpp"
//Slint stuff
#include <slint-interpreter.h>
#include "uiEventBridge.hpp"
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>

class HPRInterpreter {
    public:
        HPRInterpreter(ExtensionManager* extMgr = nullptr);
        ~HPRInterpreter();
        void show();
        void hide();
        void reload(std::string path);
        void run();
        void quit();
        UiModelManager* getModelManager() { return modelManager.has_value() ? &modelManager.value() : nullptr; }

    private:
        void trackingLoop(); // runs on separate thread so that it polls shit continously (correct spelling?)
        bool initialiseSlintUiPath();

    private:
        ExtensionManager* extManager;
        std::optional<UiEventBridge> uiEventBridge;
        std::mutex reloadMutex;

        //Interpreter stuff
        //heap allocate because we will use local compiler in reload anyway
        std::unique_ptr<slint::interpreter::ComponentCompiler> compiler;
        std::optional<slint::interpreter::ComponentDefinition> definition;
        std::optional<slint::ComponentHandle<slint::interpreter::ComponentInstance>> instance;
        std::optional<slint::ComponentWeakHandle<slint::interpreter::ComponentInstance>> weak_instance;

        std::string filePath;
        std::string fileName = "app-window.slint";

        std::atomic<bool> running{true};
        std::thread tracker;

        std::optional<UiModelManager> modelManager;

        size_t errorId;
        std::string activeGuiError = "";
        std::chrono::steady_clock::time_point errorTimestamp;
};