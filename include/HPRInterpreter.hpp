// // HPR.hpp
// #pragma once

// #include "appState.hpp"
// #include "aliasManager.hpp"
// #include "uiModelManager.hpp"

// //Slint stuff
// #include <slint-interpreter.h>

// #include <thread>
// #include <atomic>
// #include <optional>

// class HPRInterpreter {
//     public:
//         HPRInterpreter();
//         ~HPRInterpreter();
//         void show();
//         void hide();
//         void run(); // blocking call i believe
//         void quit();

//     private:
//         void trackingLoop(); // runs on separate thread so that it polls shit continously (correct spelling?)

//     private:
//         slint::ComponentHandle<MainWindow> ui;
//         std::atomic<bool> running{true};
//         std::thread tracker;

//         AliasManager aliasManager;
//         UiModelManager modelManager;
// };