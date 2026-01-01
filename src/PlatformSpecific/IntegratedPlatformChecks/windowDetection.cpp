#include "windowDetection.hpp"

#ifdef _WIN32 //Include windows headers

#include <windows.h>
#include <psapi.h>

#endif

#include <string>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "Psapi.lib")

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

static bool ContainsAlphabet(const std::string& str)
{
    for (unsigned char c : str)
    {
        if (std::isalpha(c))
            return true;
    }
    return false;
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

    return result;
}

#endif