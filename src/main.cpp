#include <iostream>
#include <thread>
#include <chrono>

#include "getCurrentWindow.hpp"

#include "app-window.h"
#include <slint.h>

#ifdef _WIN32
    #include <windows.h>
#endif

#ifdef _WIN32
    int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
    {
        (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nShowCmd;

        //Force software rendeing
        _putenv_s("SLINT_BACKEND", "winit-software");

        auto ui = MainWindow::create();
        slint::Timer timer(std::chrono::milliseconds(16), [&ui]() 
        {
            std::string current = getCurrentWindow();
            ui->set_name(slint::SharedString(current));
        });

        ui->run();
        return 0;
    }
#else
    int main()
    {
        //Force software rendeing
        setenv("SLINT_BACKEND", "winit-software", 1);

        slint::set_xdg_app_id("HPR");
        
        auto ui = MainWindow::create();
        slint::Timer timer(std::chrono::milliseconds(16), [&ui]() 
        {
            std::string current = getCurrentWindow();
            ui->set_name(slint::SharedString(current));
        });

        ui->run();
        return 0;
    }
#endif