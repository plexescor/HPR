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
#ifdef _WIN32
	#include <windows.h>

	int WINAPI WinMain(
    	HINSTANCE hInstance,
   		HINSTANCE hPrevInstance,
    	LPSTR lpCmdLine,
    	int nShowCmd)
	{

		ConfigManager conf;
		
		DatabaseManager dbm;
		dbm.run();

		ExtensionManager ext;
		AppState::extManager = &ext;
		

		//Check for hardware-acceleration flag
		if (!conf.getConfig("hardware-acceleration", true))
		{
			_putenv_s("SLINT_BACKEND", "winit-software");
		}

		//This call is non blocking, it just starts a new BG thread
		TrayManager tray;
		tray.run();

		CurrentWindowManager cwm;

		//If not to use interpreter, use inbuilt ui
		if (!conf.getConfig("use-interpreter", false))
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

		return 0;
	}

#else
	int main()
	{
		ConfigManager conf;

		//Check for hardware-acceleration flag
		//DEV NOTE: if you are on hyprland, if HA is on, the ram usage of HPR in BTOP++ can be ~250mb
		//Its not true, idk why but hyprland counts the memory used by libllvm, livnvidia and stuff
		//as used by HPR, if you get a actual breakdown of memory used by HPR, HPR will only account for only like ~30mb
		//still if it bothers you, just turn HA off in config
		if (!conf.getConfig("hardware-acceleration", true))
		{
			setenv("SLINT_BACKEND", "winit-software", 1);
		}

		DatabaseManager dbm;
		dbm.run();

		ExtensionManager ext;
		AppState::extManager = &ext;
	
		LinuxInitialiser linuxInit; //Just a utility class to create config directory and check for icon

		//This call is non blocking, it just starts a new BG thread
		TrayManager tray;
		tray.run();

		CurrentWindowManager cwm;

		//If not to use interpreter, use inbuilt ui
		if (!conf.getConfig("use-interpreter", false))
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

		return 0;
	}
#endif