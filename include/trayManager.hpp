#pragma once
#include <atomic>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

#include "appState.hpp"

#ifdef _WIN32
#include <psapi.h>
#include <shellapi.h>
#include <windows.h>
#endif

#ifdef __linux__
/* Prefer including dbus header if available; otherwise forward-declare to
   avoid includePath errors in environments where libdbus include paths
   are not configured. */
#if defined(__has_include)
#if __has_include(<dbus/dbus.h>)
#include <dbus/dbus.h>
#else
struct DBusConnection;
#endif
#else
struct DBusConnection;
#endif
#endif

class TrayManager
{
  public:
	TrayManager();
	~TrayManager();
	void run();

  public:
	std::function<void()> onQuit;
	std::function<void()> onShow;
	std::function<void()> onHide;

  private:
	void trayManager_LoopWindows();

#ifdef _WIN32
	void createTrayIcon();
	void destroyTrayIcon();
	void showContextMenu(HWND hwnd);
	static LRESULT CALLBACK trayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK hprSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static WNDPROC g_originalWndProc;
#endif

#ifdef __linux__
	void trayManager_LoopLinux();
	DBusConnection *dbusConn = nullptr;
#endif

  private:
	std::string currentPlatform;

	std::atomic<bool> running = true;
	std::thread trayThread;

#ifdef _WIN32
	HWND trayHwnd = nullptr;
	NOTIFYICONDATAW nid = {};
#endif
};