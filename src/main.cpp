#include "HPR.hpp"

#ifdef _WIN32
    #include <windows.h>

    int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
    {
        (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nShowCmd;

        //Force software rendeing
        _putenv_s("SLINT_BACKEND", "winit-software");

        HPR app;
        app.run();

        return 0;
    }
#else
    int main()
    {
        //Force software rendeing
        setenv("SLINT_BACKEND", "winit-software", 1);

        HPR app;
        app.run();

        return 0;
    }
#endif