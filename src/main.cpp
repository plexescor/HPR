/**************************************************************************
 * Human Pattern Recorder -HPR
 * Copyright © 2026 Plexescor
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * A compiled, offline, zero-account activity tracker.
 * Watches your active window. Builds your history. Never phones home.
 *
 * This [https://github.com/plexescor/HPR-Deprecated] 
 * Started as SDL3 + ImGui with a comment that said
 * "//Sorry!" in the middle of code.
 * It runs Doom now. I don't fully understand how I got here.
 *
 * Built solo. If it's been useful, a ko-fi helps:
 * https://ko-fi.com/plexescor
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.
 * See <https://www.gnu.org/licenses/>
 *
 **************************************************************************/
#include "HPR.hpp"
#include "HPRInterpreter.hpp"
#include "app-window.h"
#include "appState.hpp"
#include "autostartManager.hpp"
#include "configManager.hpp"
#include "currentWindowManager.hpp"
#include "databaseManager.hpp"
#include "extensionManager.hpp"
#include "limitsManager.hpp"
#include "linuxUtilities.hpp"
#include "singleInstance.hpp"
#include "telemetryManager.hpp"
#include "timelineManager.hpp"
#include "trayManager.hpp"
#include "windowUtilities.hpp"
#include <condition_variable>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#else
int main()
{
	//In debug mode, dont care about multiple instances
	#ifdef NDEBUG
		if (!SingleInstance::getInstance().checkAndNotify())
		{
			return 0;
		}
	#endif

	bool isAutostart = AutostartManager::isEnabled();
	AppState::configManager.setConfig("autostart", isAutostart);

	// If we are in debug, dont create a DB file
	#ifdef NDEBUG
		DatabaseManager dbm;
		dbm.run();
	#endif

	#ifdef __linux__
		LinuxInitialiser linuxInit;
	#endif

	{
		std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
		AppState::state.trackApp = AppState::configManager.getConfig<bool>("track-apps", true);
		AppState::state.trackTab = AppState::configManager.getConfig<bool>("track-browser", true);
		AppState::state.trackProject = AppState::configManager.getConfig<bool>("track-projects", true);
	}

	LimitsManager lim;
	lim.run();
	AppState::limitsManager = &lim;

	bool trueHeadless = AppState::configManager.getConfig("true-headless-mode", false);
	if (trueHeadless)
	{
		TelemetryManager::init();

		ExtensionManager ext;
		AppState::extManager = &ext;

		CurrentWindowManager cwm;

		// GIVE EXTENSION MANAGER FULL ACCESS TO EVERY OBJECT PRESENT
		#ifdef NDEBUG
			ext.dbManager = &dbm;
		#endif
		ext.trayManager = nullptr;
		ext.currentWindowManager = &cwm;
		ext.app = nullptr;
		ext.linuxInit = &linuxInit;

		cwm.detectAndSetBackend();
		ext.run();
		cwm.run();

		// block main thread indefinitely
		std::mutex mtx;
		std::unique_lock<std::mutex> lck(mtx);
		std::condition_variable cv;
		cv.wait(lck);

		SingleInstance::getInstance().shutdown();
		return 0;
	}

	TimelineManager tlm;
	tlm.run();

	TelemetryManager::init();

	ExtensionManager ext;
	AppState::extManager = &ext;

	// Check for hardware-acceleration flag
	if (!AppState::configManager.getConfig("hardware-acceleration", true))
	{
		#ifdef _WIN32
			_putenv_s("SLINT_BACKEND", "winit-software");
		#else
			setenv("SLINT_BACKEND", "winit-software", 1);
		#endif
	}

	// This call is non blocking, it just starts a new BG thread
	TrayManager tray;
	tray.run();

	CurrentWindowManager cwm;

	// If not to use interpreter, use inbuilt ui
	if (!AppState::configManager.getConfig("use-interpreter", false))
	{
		HPR app(&ext);

		tray.onQuit = [&]() { app.quit(); };

		tray.onShow = [&]() { app.show(); };
		tray.onHide = [&]() { app.hide(); };

		SingleInstance::getInstance().onShow([&]() { app.show(); });

		// GIVE EXTENSION MANAGER FULL ACCESS TO EVERY OBJECT PRESENT
		#ifdef NDEBUG
			ext.dbManager = &dbm;
		#endif
		ext.trayManager = &tray;
		ext.currentWindowManager = &cwm;
		ext.app = &app;
		ext.linuxInit = &linuxInit;

		cwm.detectAndSetBackend();
		ext.run();
		cwm.run();

		app.run(); // blocking call, run on main
	}
	// use custom gui
	else
	{
		HPRInterpreter app(&ext);
		tray.onQuit = [&]() { app.quit(); };

		tray.onShow = [&]() { app.show(); };
		tray.onHide = [&]() { app.hide(); };

		SingleInstance::getInstance().onShow([&]() { app.show(); });

		// GIVE EXTENSION MANAGER FULL ACCESS TO EVERY OBJECT PRESENT
		#ifdef NDEBUG
			ext.dbManager = &dbm;
		#endif
		ext.trayManager = &tray;
		ext.currentWindowManager = &cwm;
		ext.interpreterApp = &app;

		cwm.detectAndSetBackend();
		ext.run();
		cwm.run();

		app.run(); // blocking call, run on main
	}

	#ifdef NDEBUG
		SingleInstance::getInstance().shutdown();
	#endif
	return 0;
}

#endif