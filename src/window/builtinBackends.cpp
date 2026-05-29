#include "windowBackendRegistery.hpp"
#include "windowUtilities.hpp"
#include <string>
#include <iostream>
#include <filesystem>
#ifdef _WIN32
    #include <windows.h>
    #include <Psapi.h>
#endif
// #include <fstream>

//fuck you kde
// #ifdef __linux__
//     #include <dbus/dbus.h>
//     #include <atomic>
//     #include <thread>
//     #include <mutex>
// #endif

static std::string s_qdbus_bin = "";

std::string getQDBusCommand() 
{
    if (!s_qdbus_bin.empty()) return s_qdbus_bin;
    
    std::string cmd = "command -v qdbus6 || command -v qdbus-qt6 || command -v qdbus";
    std::string check = runSystemCommand(cmd);
    
    // Trim whitespace/newlines
    while (!check.empty() && (check.back() == '\n' || check.back() == ' '))
        check.pop_back();
        
    s_qdbus_bin = check.empty() ? "qdbus6" : check;
    return s_qdbus_bin;
}

void registerBuiltinBackends()
{
    registerBackend
    ({
        "GNOME",
        [](const std::string& desktopEnvironment)
        {
            return desktopEnvironment.contains("GNOME");
        },

        []()
        {
            std::string extensionDirectory =
                std::string(getenv("HOME")) +
                "/.local/share/gnome-shell/extensions/"
                "lol-another-window-extension@plexescor";

            bool extensionFilesExist =
                std::filesystem::exists(extensionDirectory);

            std::string extensionCheckCommand =
                "gdbus call --session "
                "--dest org.gnome.Shell "
                "--object-path "
                "/org/gnome/Shell/Extensions/"
                "LolAnotherWindowExtension "
                "--method "
                "org.gnome.Shell.Extensions."
                "LolAnotherWindowExtension.FocusClass 2>&1";

            std::string extensionCheckResult =
                runSystemCommand(extensionCheckCommand);

            std::cout
                << "[HPR] GNOME extension check: "
                << extensionCheckResult
                << std::endl;

            bool extensionWorking =
                extensionCheckResult.contains("('");

            if (!extensionWorking && extensionFilesExist) 
            {
                std::cout
                    << "[HPR] Enabling GNOME extension..."
                    << std::endl;

                std::string cmd = "gnome-extensions enable "
                    "lol-another-window-extension@plexescor";
                runSystemCommand(cmd);
            }
        },

        []()
        {
            std::string extensionCheckCommand =
                "gdbus call --session "
                "--dest org.gnome.Shell "
                "--object-path "
                "/org/gnome/Shell/Extensions/"
                "LolAnotherWindowExtension "
                "--method "
                "org.gnome.Shell.Extensions."
                "LolAnotherWindowExtension.FocusClass 2>&1";

            std::string result =
                runSystemCommand(extensionCheckCommand);

            return result.contains("('");
        },
        []() -> std::string
        {
            std::string command =
                "gdbus call --session --dest org.gnome.Shell --object-path "
                "/org/gnome/Shell/Extensions/LolAnotherWindowExtension --method "
                "org.gnome.Shell.Extensions.LolAnotherWindowExtension.FocusClass";

            std::string result = runSystemCommand(command);

            size_t start = result.find('\'');
            size_t end = result.rfind('\'');

            if (start != std::string::npos &&
                end != std::string::npos &&
                start != end)
            {
                return result.substr(start + 1, end - start - 1);
            }

            return "";
        },

        []() -> std::string
        {
            std::string command =
                "gdbus call --session --dest org.gnome.Shell --object-path "
                "/org/gnome/Shell/Extensions/LolAnotherWindowExtension --method "
                "org.gnome.Shell.Extensions.LolAnotherWindowExtension.FocusTitle";

            std::string result = runSystemCommand(command);

            size_t start = result.find('\'');
            size_t end = result.rfind('\'');

            if (start != std::string::npos &&
                end != std::string::npos &&
                start != end)
            {
                return result.substr(start + 1, end - start - 1);
            }

            return "";
        }
    });

    registerBackend({
        "KDE",
        [](const std::string& env) { return env.contains("KDE"); },

        []() {}, 

        []() { return !getQDBusCommand().empty(); }, // isUsable

        []() -> std::string 
        {
            std::string cmd = "kdotool getactivewindow getwindowclassname";
            std::string result = runSystemCommand(cmd);
            
            while (!result.empty() && (result.back() == '\n' || result.back() == ' '))
                result.pop_back();
                
            return result;
        },

        []() -> std::string 
        {
            std::string cmd = "kdotool getactivewindow getwindowname";
            return runSystemCommand(cmd);
        }

    });

    registerBackend
    ({
        "Hyprland",

        // matchesEnvironment
        [](const std::string& env) 
        {
            return env.contains("Hyprland");
        },

        []()
        {
            //no shit
        },

        []()
        {
            std::string cmd = "hyprctl -j activewindow";
            std::string result =
                runSystemCommand(cmd);

            return result.contains("class");
        },

        []() -> std::string
        {
            std::string cmd = "hyprctl activewindow -j";
            std::string json = runSystemCommand(cmd);
            
            // Find "class":"value"
            const std::string key = "\"class\":";
            size_t keyPos = json.find(key);
            if (keyPos == std::string::npos) return "";
            
            size_t quoteStart = json.find('"', keyPos + key.size());
            if (quoteStart == std::string::npos) return "";
            
            size_t quoteEnd = json.find('"', quoteStart + 1);
            if (quoteEnd == std::string::npos) return "";
            
            return json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        },
    
        []() -> std::string
        {
            std::string cmd = "hyprctl activewindow -j";
            std::string json = runSystemCommand(cmd);
            
            // Find "title":"value"
            const std::string key = "\"title\":";
            size_t keyPos = json.find(key);
            if (keyPos == std::string::npos) return "";
            
            size_t quoteStart = json.find('"', keyPos + key.size());
            if (quoteStart == std::string::npos) return "";
            
            size_t quoteEnd = json.find('"', quoteStart + 1);
            if (quoteEnd == std::string::npos) return "";
            
            return json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        }

    });

    registerBackend
    ({
        "Windows",

        // matchesEnvironment
        [](const std::string& env) 
        {
            #ifdef _WIN32
                return env.contains("Windows");
            #endif
            return false;
        },

        []()
        {
            //no shit
        },

        []()
        {
            #ifdef _WIN32
                HWND hwnd = GetForegroundWindow();

                DWORD pid = 0;
                GetWindowThreadProcessId(hwnd, &pid);

                HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

                char procName[MAX_PATH] = {};
                DWORD size = MAX_PATH;
                QueryFullProcessImageNameA(hProcess, 0, procName, &size);
                CloseHandle(hProcess);

                // Extract just the filename from full path
                std::string fullPath(procName);
                size_t lastSlash = fullPath.find_last_of("\\/");
                std::string filename = (lastSlash != std::string::npos) ? fullPath.substr(lastSlash + 1) : fullPath;

                //if it contains .exe, then yes its ready or whateevr
                return filename.contains(".exe");
            #endif
                return false;
        },


        []() -> std::string
        {
            #ifdef _WIN32
                HWND hwnd = GetForegroundWindow();
                if (!hwnd)
                    return "";

                DWORD pid = 0;
                GetWindowThreadProcessId(hwnd, &pid);
                if (!pid)
                    return "";

                HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                if (!hProcess)
                    return "";

                char procName[MAX_PATH] = {};
                DWORD size = MAX_PATH;
                QueryFullProcessImageNameA(hProcess, 0, procName, &size);
                CloseHandle(hProcess);

                // Extract just the filename from full path
                std::string fullPath(procName);
                size_t lastSlash = fullPath.find_last_of("\\/");
                std::string filename = (lastSlash != std::string::npos) ? fullPath.substr(lastSlash + 1) : fullPath;

                // Strip .exe
                if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".exe")
                    filename = filename.substr(0, filename.size() - 4);

                return filename;
            #endif
                return "";
        },

        []() -> std::string
        {
            #ifdef _WIN32
                HWND hwnd = GetForegroundWindow();
                if (!hwnd)
                    return "";

                wchar_t title[512] = {};
                GetWindowTextW(hwnd, title, sizeof(title) / sizeof(wchar_t));

                std::wstring wstr(title);
                if (wstr.empty())
                    return "";

                int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
                std::string strTo(size_needed, 0);
                WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);

                return strTo;
            #endif
                return "";
        }

    });

    registerBackend
    ({
        "Cinnamon",

        // matchesEnvironment
        [](const std::string& env) 
        {
            return env.contains("Cinnamon");
        },

        []()
        {
            //no shit
        },

        []()
        {
            std::string cmd = "gdbus call --session --dest org.Cinnamon --object-path /org/Cinnamon --method org.Cinnamon.Eval \"global.display.focus_window.get_wm_class()\"";
            std::string rawOutput = runSystemCommand(cmd);

            return rawOutput.contains("('");
        },


        []() -> std::string
        {
            std::string cmd = "gdbus call --session --dest org.Cinnamon --object-path /org/Cinnamon --method org.Cinnamon.Eval \"global.display.focus_window.get_wm_class()\"";
            std::string rawOutput = runSystemCommand(cmd);
            // Find the positions of the single-quotes wrapping the inner string
            size_t startQuote = rawOutput.find('\'');
            size_t endQuote = rawOutput.rfind('\'');
            
            if (startQuote == std::string::npos || endQuote == std::string::npos || startQuote >= endQuote) {
                return ""; // return empty if quotes aren't matched
            }
            
            // Extract everything between the single quotes
            std::string inner = rawOutput.substr(startQuote + 1, endQuote - startQuote - 1);
            
            // strip the literal double-quotes if they exist
            if (inner.length() >= 2 && inner.front() == '"' && inner.back() == '"') {
                inner = inner.substr(1, inner.length() - 2);
            }
            
            // trim any trailing newlines or extra whitespaces
            while (!inner.empty() && (inner.back() == '\n' || inner.back() == '\r' || inner.back() == ' ')) {
                inner.pop_back();
            }

            return inner;
        },
    
        []() -> std::string
        {
            std::string cmd = "gdbus call --session --dest org.Cinnamon --object-path /org/Cinnamon --method org.Cinnamon.Eval 'global.get_window_actors().filter(a => a.meta_window.has_focus())[0].get_meta_window().get_title()'";
            
            std::string rawOutput = runSystemCommand(cmd);

            // Find the positions of the single-quotes wrapping the inner string
            size_t startQuote = rawOutput.find('\'');
            size_t endQuote = rawOutput.rfind('\'');
            
            if (startQuote == std::string::npos || endQuote == std::string::npos || startQuote >= endQuote) {
                return ""; // return empty if quotes aren't matched
            }
            
            // Extract everything between the single quotes
            std::string inner = rawOutput.substr(startQuote + 1, endQuote - startQuote - 1);
            
            // strip the literal double-quotes if they exist
            if (inner.length() >= 2 && inner.front() == '"' && inner.back() == '"') {
                inner = inner.substr(1, inner.length() - 2);
            }
            
            // trim any trailing newlines or extra whitespaces
            while (!inner.empty() && (inner.back() == '\n' || inner.back() == '\r' || inner.back() == ' ')) {
                inner.pop_back();
            }

            return inner;
        }
    });
}