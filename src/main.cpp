#include "app-window.h"
#include "databaseManager.hpp"
#include "getCurrentWindow.hpp"
#include "HPR.hpp"
#include "trayManager.hpp"
#include "configManager.hpp"
#include "HPRInterpreter.hpp"
#include "linuxUtilities.hpp"
#include "extensionManager.hpp"
#include "windowUtilities.hpp"
#include "appState.hpp"
#include "limitsManager.hpp"
#include "timelineManager.hpp"
#include "singleInstance.hpp"
#include "autostartManager.hpp"
#include "telemetryManager.hpp"
#include "nowPlayingManager.hpp"

#ifdef _WIN32
	#include <windows.h>

	int WINAPI WinMain(
    	HINSTANCE hInstance,
    	HINSTANCE hPrevInstance,
    	LPSTR lpCmdLine,
    	int nShowCmd)
	{
		if (!SingleInstance::getInstance().checkAndNotify())
		{
			return 0;
		}

		bool isAutostart = AutostartManager::isEnabled();
		AppState::configManager.setConfig("autostart", isAutostart);

		DatabaseManager dbm;
		dbm.run();

		LimitsManager lim;
		lim.run();

		TimelineManager tlm;
		tlm.run();

		TelemetryManager::init();
		NowPlayingManager::init();

		ExtensionManager ext;
		AppState::extManager = &ext;

		//Check for hardware-acceleration flag
		if (!AppState::configManager.getConfig("hardware-acceleration", true))
		{
			_putenv_s("SLINT_BACKEND", "winit-software");
		}

		//This call is non blocking, it just starts a new BG thread
		TrayManager tray;
		tray.run();

		CurrentWindowManager cwm;

		//If not to use interpreter, use inbuilt ui
		if (!AppState::configManager.getConfig("use-interpreter", false))
		{
			HPR app(&ext);
		
			tray.onQuit = [&]() {
				app.quit();
			};

			tray.onShow = [&]() {
				app.show();
			};
			tray.onHide = [&]() {
				app.hide();
			};

			SingleInstance::getInstance().onShow([&]() {
				app.show();
			});

			//GIVE EXTENSION MANAGER FULL ACCESS TO EVERY OBJECT PRESENT
			ext.dbManager = &dbm;
			ext.trayManager = &tray;
			ext.currentWindowManager = &cwm;
			ext.app = &app;

			ext.run();

			cwm.detectAndSetBackend();
			cwm.run();

			app.run(); //blocking call, run on main
		}	
		//use custom gui
		else
		{
			HPRInterpreter app(&ext);
			tray.onQuit = [&]() {
				app.quit();
			};

			tray.onShow = [&]() {
				app.show();
			};
			tray.onHide = [&]() {
				app.hide();
			};

			SingleInstance::getInstance().onShow([&]() {
				app.show();
			});

			//GIVE EXTENSION MANAGER FULL ACCESS TO EVERY OBJECT PRESENT
			ext.dbManager = &dbm;
			ext.trayManager = &tray;
			ext.currentWindowManager = &cwm;
			ext.interpreterApp = &app;			
			
			ext.run();
			cwm.detectAndSetBackend();
			cwm.run();

			app.run();//blocking call, run on main
		}

		SingleInstance::getInstance().shutdown();
		return 0;
	}

#else
	int main()
	{
		if (!SingleInstance::getInstance().checkAndNotify())
		{
			return 0;
		}

		bool isAutostart = AutostartManager::isEnabled();
		AppState::configManager.setConfig("autostart", isAutostart);

		//Check for hardware-acceleration flag
		//DEV NOTE: if you are on hyprland, if HA is on, the ram usage of HPR in BTOP++ can be ~250mb
		//Its not true, idk why but hyprland counts the memory used by libllvm, livnvidia and stuff
		//as used by HPR, if you get a actual breakdown of memory used by HPR, HPR will only account for only like ~30mb
		//still if it bothers you, just turn HA off in config
		if (!AppState::configManager.getConfig("hardware-acceleration", true))
		{
			setenv("SLINT_BACKEND", "winit-software", 1);
		}

		DatabaseManager dbm;
		dbm.run();

		LimitsManager lim;
		lim.run();

		TimelineManager tlm;
		tlm.run();

		TelemetryManager::init();


		ExtensionManager ext;
		AppState::extManager = &ext;
	
		LinuxInitialiser linuxInit; //Just a utility class to create config directory and check for icon

		//This call is non blocking, it just starts a new BG thread
		TrayManager tray;
		tray.run();

		CurrentWindowManager cwm;

		//If not to use interpreter, use inbuilt ui
		if (!AppState::configManager.getConfig("use-interpreter", false))
		{
			HPR app(&ext);

			tray.onQuit = [&]() {
				app.quit();
			};
			tray.onShow = [&]() {
				app.show();
			};
			tray.onHide = [&]() {
				app.hide();
			};

			SingleInstance::getInstance().onShow([&]() {
				app.show();
			});

			//GIVE EXTENSION MANAGER FULL ACCESS TO EVERY OBJECT PRESENT
			ext.dbManager = &dbm;
			ext.trayManager = &tray;
			ext.currentWindowManager = &cwm;
			ext.app = &app;
			ext.linuxInit = &linuxInit;

			ext.run();

			cwm.detectAndSetBackend();
			cwm.run();

			app.run();//blocking call, run on main
		}	
		//use custom gui
		else
		{
			HPRInterpreter app(&ext);

			tray.onQuit = [&]() {
				app.quit();
			};
			tray.onShow = [&]() {
				app.show();
			};
			tray.onHide = [&]() {
				app.hide();
			};

			SingleInstance::getInstance().onShow([&]() {
				app.show();
			});

			//GIVE EXTENSION MANAGER FULL ACCESS TO EVERY OBJECT PRESENT
			ext.dbManager = &dbm;
			ext.trayManager = &tray;
			ext.currentWindowManager = &cwm;
			ext.interpreterApp = &app;
			ext.linuxInit = &linuxInit;

			ext.run();

			cwm.detectAndSetBackend();
			cwm.run();

			app.run();//blocking call, run on main
		}

		SingleInstance::getInstance().shutdown();
		return 0;
	}
#endif