#include "windowDetection.hpp"
#include "validateWindow.hpp"

#ifdef _WIN32 //Include windows headers

#include <windows.h>
#include <psapi.h>

#endif

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xatom.h>   // Important
#endif

#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "Psapi.lib")


static bool ContainsAlphabet(const std::string& str)
{
    for (unsigned char c : str)
    {
        if (std::isalpha(c))
            return true;
    }
    return false;
}

//FUNCTION DEFINITION FOR WINDOWS

#ifdef _WIN32

static void StripExeExtension(std::string& name)
{
    if (name.length() >= 4)
    {
        std::string tail = name.substr(name.length() - 4);
        std::transform(tail.begin(), tail.end(), tail.begin(), ::tolower);

        if (tail == ".exe")
            name.erase(name.length() - 4);
    }
}



std::string getActiveProcessName()
{
    // 1. Get active (foreground) window
    HWND hwnd = GetForegroundWindow();
    if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd))
        return "";

    // 2. Get process ID from window
    DWORD pid = 0;
    if (GetWindowThreadProcessId(hwnd, &pid) == 0 || pid == 0)
        return "";

    // 3. Open process with minimal required access
    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
        FALSE,
        pid
    );

    if (!hProcess)
        return "";

    // 4. Get process name
    char processName[MAX_PATH] = { 0 };

    DWORD len = GetModuleBaseNameA(
        hProcess,
        nullptr,
        processName,
        MAX_PATH
    );

    CloseHandle(hProcess);

    if (len == 0 || processName[0] == '\0')
        return "";

    std::string result(processName);

    // 5. Strip ".exe"
    StripExeExtension(result);

    // 6. Reject junk names (must contain at least one alphabet)
    if (!ContainsAlphabet(result))
        return "";

    //Before returning result, we must do some *important*checks and potentially some modifications

    result = updateWindowName(result);
    return result;
}

#endif

//Function definition for Linux
#ifdef __linux__

std::string getActiveProcessName()
{
    // 1. Connect to X11 display
    Display* display = XOpenDisplay(nullptr);
    if (!display)
        return "";

    // 2. Get foreground window
    Window root = DefaultRootWindow(display);
    Atom activeWindowAtom = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    
    Atom actualType;
    int actualFormat;
    unsigned long nitems, bytesAfter;
    unsigned char* prop = nullptr;
    
    if (XGetWindowProperty(display, root, activeWindowAtom, 0, 1, False,
                          XA_WINDOW, &actualType, &actualFormat, &nitems,
                          &bytesAfter, &prop) != Success || !prop || nitems == 0)
    {
        if (prop) XFree(prop);
        XCloseDisplay(display);
        return "";
    }

    Window window = *(Window*)prop;
    XFree(prop);

    // 3. Get process ID from window
    Atom wmPidAtom = XInternAtom(display, "_NET_WM_PID", False);
    prop = nullptr;
    
    if (XGetWindowProperty(display, window, wmPidAtom, 0, 1, False,
                          XA_CARDINAL, &actualType, &actualFormat, &nitems,
                          &bytesAfter, &prop) != Success || !prop || nitems == 0)
    {
        if (prop) XFree(prop);
        XCloseDisplay(display);
        return "";
    }

    pid_t pid = *(pid_t*)prop;
    XFree(prop);
    XCloseDisplay(display);

    if (pid <= 0)
        return "";

    // 4. Get process name from /proc/[pid]/comm
    std::string commPath = "/proc/" + std::to_string(pid) + "/comm";
    std::ifstream commFile(commPath);
    
    if (!commFile.is_open())
        return "";

    std::string processName;
    if (!std::getline(commFile, processName) || processName.empty())
        return "";

    commFile.close();

    // 5. Remove trailing newline if present
    if (!processName.empty() && processName.back() == '\n')
        processName.pop_back();

    // 6. Reject junk names (must contain at least one alphabet)
    if (!ContainsAlphabet(processName))
        return "";

    // Before returning result, do some important checks and modifications
    processName = updateWindowName(processName);
    return processName;
}

#endif