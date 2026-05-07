#include "app-window.h"
#include "databaseManager.hpp"
#include "getCurrentWindow.hpp"
#include "HPR.hpp"
#include "trayManager.hpp"
#include "configManager.hpp"
#include "HPRInterpreter.hpp"

#ifdef _WIN32
	#include <windows.h>

	int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
					LPSTR lpCmdLine, int nShowCmd)
	{
		(void)hInstance;
		(void)hPrevInstance;
		(void)lpCmdLine;
		(void)nShowCmd;

		// Force software rendeing
		// _putenv_s("SLINT_BACKEND", "winit-software");

		ConfigManager conf;
		TrayManager tray;
		tray.run();


		DatabaseManager dbm;
		CurrentWindowManager cwm;
		
		dbm.run();
		cwm.run();

		//If not to use interpreter, use inbuilt ui
		if (!conf.getConfig("use-interpreter", false))
		{
			HPR app;
		
			tray.onQuit = [&]() {
				app.quit();
			};

			tray.onShow = [&]() {
				app.show();
			};

			app.run();
		}	
		//use custom gui
		else
		{
			HPRInterpreter app;
			tray.onQuit = [&]() {
				app.quit();
			};

			tray.onShow = [&]() {
				app.show();
			};

			app.run();
		}

		return 0;
	}

#else
	int main()
	{
		// Force software rendeing
		setenv("SLINT_BACKEND", "winit-software", 1);

		DatabaseManager dbm;
		CurrentWindowManager cwm;

		dbm.run();
		cwm.run();

		//If not to use interpreter, use inbuilt ui
		if (!conf.getConfig("use-interpreter", false))
		{
			HPR app;
			app.run();
		}	
		//use custom gui
		else
		{
			HPRInterpreter app;
			app.run();
		}

		return 0;
	}
#endif