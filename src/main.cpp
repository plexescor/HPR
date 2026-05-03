#include "HPR.hpp"
#include "getCurrentWindow.hpp"
#include "databaseManager.hpp"

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
		_putenv_s("SLINT_BACKEND", "winit-software");

		CurrentWindowManager cwm;
		cwm.run();

		DatabaseManager dbm;
		dbm.run();

		HPR app;
		app.run();

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

		HPR app;
		app.run();

		return 0;
	}
#endif