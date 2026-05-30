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

//////////////////////////////////////////////////////////////////////////////////////////////////////
///
///
///
///         MOTHERFUCKING KDE YOU ARE WORSE THAN GNOME I WILL FUK YOU SO HARD YU WILL 
///         REGRET THE DAY YOU WERE BORN, I SWEAR TO GOD, I WILL FUCKING END YOU
///         
///
///
///////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef __linux__
    #include <dbus/dbus.h>
    #include <functional>
    #include <chrono>
    #include <thread>
    #include <random>
    //ATTENTION GUYS
    //I JUST YOINKED KDOTOOL SRC CODE
    //AND USED CLAUDE TO TRANSLATE TO CPP CODE
    //BECAUSE IT WAS IN FUCKIN RUST

    namespace MotherfuckingKDE
    {
        // Generates a unique marker/dbus addr like kdotool does
        std::string generateMarker() 
        {
            std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
            std::uniform_int_distribution<int> dist(100000, 999999);
            return "kdotool_cpp_" + std::to_string(dist(rng));
        }

        // The JS that gets injected into KWin - mirrors kdotool's SCRIPT_HEADER + steps + SCRIPT_FOOTER
        std::string buildScript(const std::string& dbusAddr, const std::string& marker, const std::string& action)
        {
            return R"(
                function output_result(message) {
                    if (message == null) message = "null";
                    callDBus(")" + dbusAddr + R"(", "/", "", "result", message.toString());
                }
                function output_error(message) {
                    callDBus(")" + dbusAddr + R"(", "/", "", "error", message.toString());
                }
                workspace_activeWindow = () => workspace.activeWindow;
                function run() {
                    var window_stack = [workspace_activeWindow()];
                    if (window_stack.length > 0) {
                        let w = window_stack[0];
                        )" + action + R"(
                    }
                }
                run();
            )";
        }

        // Loads and runs a script via KWin D-Bus scripting interface
        // mirrors what kdotool does: loadScript - start - unloadScript
        bool kwinRunScript(DBusConnection* conn, const std::string& scriptContent, int& scriptId)
        {
            // Call org.kde.kwin.Scripting.loadScript(script_content, script_name)
            DBusMessage* msg = dbus_message_new_method_call(
                "org.kde.KWin",
                "/Scripting",
                "org.kde.kwin.Scripting",
                "loadScript"
            );

            const char* content = scriptContent.c_str();
            const char* name = "kdotool_cpp_tmp";
            dbus_message_append_args(msg,
                DBUS_TYPE_STRING, &content,
                DBUS_TYPE_STRING, &name,
                DBUS_TYPE_INVALID);

            DBusError err;
            dbus_error_init(&err);
            DBusMessage* reply = dbus_connection_send_with_reply_and_block(conn, msg, 3000, &err);
            dbus_message_unref(msg);

            if (!reply || dbus_error_is_set(&err)) return false;

            dbus_message_get_args(reply, &err, DBUS_TYPE_INT32, &scriptId, DBUS_TYPE_INVALID);
            dbus_message_unref(reply);

            // Call start() on the script object
            std::string scriptPath = "/Scripting/Script" + std::to_string(scriptId);
            msg = dbus_message_new_method_call(
                "org.kde.KWin",
                scriptPath.c_str(),
                "org.kde.kwin.Script",
                "run"
            );
            reply = dbus_connection_send_with_reply_and_block(conn, msg, 3000, &err);
            dbus_message_unref(msg);
            if (reply) dbus_message_unref(reply);

            return true;
        }

        void kwinUnloadScript(DBusConnection* conn, int scriptId)
        {
            DBusMessage* msg = dbus_message_new_method_call(
                "org.kde.KWin",
                "/Scripting",
                "org.kde.kwin.Scripting",
                "unloadScript"
            );
            const char* name = "kdotool_cpp_tmp";
            dbus_message_append_args(msg, DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID);
            DBusError err;
            dbus_error_init(&err);
            DBusMessage* reply = dbus_connection_send_with_reply_and_block(conn, msg, 3000, &err);
            if (reply) dbus_message_unref(reply);
            dbus_message_unref(msg);
        }

        // Register a temporary D-Bus service to receive results back from KWin script
        // mirrors kdotool's result collection via its own dbus_addr
        std::string kwinScriptGetResult(const std::string& dbusAddr, const std::string& action)
        {
            DBusError err;
            dbus_error_init(&err);

            DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
            if (!conn || dbus_error_is_set(&err)) return "";

            // Request our temporary bus name so KWin script can call us back
            int ret = dbus_bus_request_name(conn, dbusAddr.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
            if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) return "";

            std::string marker = generateMarker();
            std::string script = buildScript(dbusAddr, marker, action);

            int scriptId = -1;
            if (!kwinRunScript(conn, script, scriptId)) {
                dbus_bus_release_name(conn, dbusAddr.c_str(), &err);
                return "";
            }

            // Poll for result message - KWin script calls us back via callDBus
            std::string result;
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

            while (std::chrono::steady_clock::now() < deadline) {
                dbus_connection_read_write(conn, 100);
                DBusMessage* incoming = dbus_connection_pop_message(conn);
                if (!incoming) continue;

                if (dbus_message_is_method_call(incoming, "", "result")) {
                    const char* val = nullptr;
                    dbus_message_get_args(incoming, &err, DBUS_TYPE_STRING, &val, DBUS_TYPE_INVALID);
                    if (val) result = val;

                    // Send empty reply so KWin doesn't hang
                    DBusMessage* replyMsg = dbus_message_new_method_return(incoming);
                    dbus_connection_send(conn, replyMsg, nullptr);
                    dbus_message_unref(replyMsg);
                    dbus_message_unref(incoming);
                    break;
                }
                dbus_message_unref(incoming);
            }

            kwinUnloadScript(conn, scriptId);
            dbus_bus_release_name(conn, dbusAddr.c_str(), &err);

            return result;
        }

        std::string kdeGetActiveWindowClass()
        {
            std::string addr = "org.kde.kdotool_cpp." + generateMarker();
            return kwinScriptGetResult(addr, "output_result(w.resourceClass);");
        }

        std::string kdeGetActiveWindowName()
        {
            std::string addr = "org.kde.kdotool_cpp." + generateMarker();
            return kwinScriptGetResult(addr, "output_result(w.caption);");
        }
    }
#endif

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
            std::string result = MotherfuckingKDE::kdeGetActiveWindowClass();
            while (!result.empty() && (result.back() == '\n' || result.back() == ' '))
                result.pop_back();
            return result;
        },

        []() -> std::string 
        {
            std::string result = MotherfuckingKDE::kdeGetActiveWindowName();
            while (!result.empty() && (result.back() == '\n' || result.back() == ' '))
                result.pop_back();
            return result;
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