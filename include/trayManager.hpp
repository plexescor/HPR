#pragma once
#include <iostream>
#include <string>
#include <thread>

#include "appState.hpp"

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
#endif

class TrayManager
{
    public:
        TrayManager();
        ~TrayManager();
        void run();

    private:
        void trayManager_LoopWindows();

        #ifdef _WIN32
            void createTrayIcon();
            void destroyTrayIcon();
            void showContextMenu(HWND hwnd);
            static LRESULT CALLBACK trayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        #endif

    private:
        std::string currentPlatform;

        std::atomic<bool> running = true;
        std::thread trayThread;

        #ifdef _WIN32
            HWND trayHwnd = nullptr;
            NOTIFYICONDATA nid = {};
        #endif
};