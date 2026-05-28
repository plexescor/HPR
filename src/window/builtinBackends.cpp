#include "windowBackendRegistery.hpp"
#include "windowUtilities.hpp"
#include <string>
#include <iostream>
#include <filesystem>
#ifdef _WIN32
    #include <windows.h>
    #include <Psapi.h>
#endif

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

    registerBackend
    ({
        "KDE",

        // matchesEnvironment
        [](const std::string& env)
        {
            return env.contains("KDE");
        },

        // initialize
        []()
        {
            std::string cmd =
                "command -v qdbus6 || "
                "command -v qdbus-qt6 || "
                "command -v qdbus";

            std::string result = runSystemCommand(cmd);

            if (!result.empty() && result.back() == '\n')
                result.pop_back();

            if (result.empty())
                result = "qdbus6";

            std::cout
                << "[HPR] KDE using: "
                << result
                << std::endl;
        },

        // isUsable
        []()
        {
            std::string cmd = "command -v qdbus6 || command -v qdbus-qt6 || command -v qdbus";
            std::string result =
                runSystemCommand(cmd);

            return !result.empty();
        },

        // getCurrentWindow
        []() -> std::string
        {
            static std::string qdbus =
                []()
                {
                    std::string cmd =
                        "command -v qdbus6 || "
                        "command -v qdbus-qt6 || "
                        "command -v qdbus";

                    std::string r = runSystemCommand(cmd);

                    if (!r.empty() && r.back() == '\n')
                        r.pop_back();

                    if (r.empty())
                        return std::string("qdbus6");

                    return r;
                }();

            std::string cmd =
                "echo 'print(workspace.activeWindow.resourceClass);' > /tmp/kwin_active.js && "
                "S=$(" + qdbus + " org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript /tmp/kwin_active.js kwin_tmp_$$) && "
                "T=$(date '+%Y-%m-%d %H:%M:%S') && "
                + qdbus + " org.kde.KWin /Scripting/Script$S org.kde.kwin.Script.run > /dev/null 2>&1 && "
                "sleep 0.1 && "
                "journalctl --since \"$T\" -o cat | grep '^js:' | tail -n 1 | sed 's/^js: //' ; "
                + qdbus + " org.kde.KWin /Scripting/Script$S org.kde.kwin.Script.stop > /dev/null 2>&1 ; "
                + qdbus + " org.kde.KWin /Scripting unloadScript kwin_tmp_$$ > /dev/null 2>&1";

            std::string result = runSystemCommand(cmd);

            while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
                result.pop_back();

            const std::string prefix = "js: ";
            if (result.starts_with(prefix))
                result = result.substr(prefix.size());

            return result;
        },

        // getCurrentTitle
        []() -> std::string
        {
            static std::string qdbus =
                []()
                {
                    std::string cmd =
                        "command -v qdbus6 || "
                        "command -v qdbus-qt6 || "
                        "command -v qdbus";

                    std::string r = runSystemCommand(cmd);

                    if (!r.empty() && r.back() == '\n')
                        r.pop_back();

                    if (r.empty())
                        return std::string("qdbus6");

                    return r;
                }();

            std::string cmd =
                "echo 'print(workspace.activeWindow.caption);' > /tmp/kwin_active.js && "
                "S=$(" + qdbus + " org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript /tmp/kwin_active.js kwin_tmp_$$) && "
                "T=$(date '+%Y-%m-%d %H:%M:%S') && "
                + qdbus + " org.kde.KWin /Scripting/Script$S org.kde.kwin.Script.run > /dev/null 2>&1 && "
                "sleep 0.1 && "
                "journalctl --since \"$T\" -o cat | grep '^js:' | tail -n 1 | sed 's/^js: //' ; "
                + qdbus + " org.kde.KWin /Scripting/Script$S org.kde.kwin.Script.stop > /dev/null 2>&1 ; "
                + qdbus + " org.kde.KWin /Scripting unloadScript kwin_tmp_$$ > /dev/null 2>&1";

            std::string result = runSystemCommand(cmd);

            while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
                result.pop_back();

            const std::string prefix = "js: ";
            if (result.starts_with(prefix))
                result = result.substr(prefix.size());

            return result;
        }
    });

    registerBackend
    ({
        "Hyprland",
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