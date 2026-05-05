#include "trayManager.hpp"

#include <iostream>
#include <string>
#include <thread>
#include <mutex>

#include "appState.hpp"

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
#endif

TrayManager::TrayManager()
{
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        currentPlatform = AppState::state.currentPlatform;
    }
    
}

TrayManager::~TrayManager()
{
    running = false;
    if (trayThread.joinable()) trayThread.join();
    
    #ifdef _WIN32
        destroyTrayIcon();
    #endif
}

void TrayManager::run()
{
    #ifdef _WIN32
        trayThread = std::thread(&TrayManager::trayManager_LoopWindows, this);
    #endif
}

#ifdef _WIN32

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_QUIT 1001
#define ID_TRAY_SHOW 1002

// Static pointer so wndproc can access instance
static TrayManager* g_trayInstance = nullptr;

LRESULT CALLBACK TrayManager::trayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP)
            {
                if (g_trayInstance)
                    g_trayInstance->showContextMenu(hwnd);
            }
            return 0;

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_TRAY_QUIT)
                PostQuitMessage(0);
            else if (LOWORD(wParam) == ID_TRAY_SHOW)
            {
                // Find and show HPR window
                HWND hprWnd = FindWindowW(NULL, L"HPR");
                if (hprWnd)
                {
                    ShowWindow(hprWnd, SW_RESTORE);
                    SetForegroundWindow(hprWnd);
                }
            }
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

void TrayManager::createTrayIcon()
{
    // Register a hidden window class to receive tray messages
    WNDCLASSW wc = {};
    wc.lpfnWndProc = trayWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"HPRTrayClass";
    RegisterClassW(&wc);

    // Create invisible message-only window
    trayHwnd = CreateWindowW(L"HPRTrayClass", L"HPR Tray",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);

    // Load icon from embedded resource
    HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));

    // Fill NOTIFYICONDATA
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = trayHwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hIcon;
    wcscpy_s(nid.szTip, L"HPR - Human Pattern Recorder");

    Shell_NotifyIcon(NIM_ADD, &nid);
}

void TrayManager::destroyTrayIcon()
{
    Shell_NotifyIcon(NIM_DELETE, &nid);
    if (trayHwnd) DestroyWindow(trayHwnd);
}

void TrayManager::showContextMenu(HWND hwnd)
{
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW, L"Show HPR");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_TRAY_QUIT, L"Quit");

    // Get cursor position for menu placement
    POINT pt;
    GetCursorPos(&pt);

    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(menu);
}

void TrayManager::trayManager_LoopWindows()
{
    g_trayInstance = this;
    createTrayIcon();

    // Message loop for tray window
    MSG msg;
    while (running && GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    destroyTrayIcon();
}

#endif