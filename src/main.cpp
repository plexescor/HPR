#include <iostream>
#include <thread>
#include <chrono>

#include "getCurrentWindow.hpp"

#include "app-window.h"
#include <slint.h>

int main()
{
    //Force software rendeing
    setenv("SLINT_BACKEND", "winit-software", 1);

    #ifdef __linux__
    slint::set_xdg_app_id("HPR");
    #endif
    
    auto ui = MainWindow::create();
    slint::Timer timer(std::chrono::milliseconds(16), [&ui]() 
    {
        std::string current = getCurrentWindow();
        ui->set_name(slint::SharedString(current));
    });

    ui->run();
    return 0;
}