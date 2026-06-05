#include "windowBackendRegistery.hpp"
#include "windowUtilities.hpp"
#include <string>
#include <iostream>
#include "logger.hpp"
#include <filesystem>
#ifdef _WIN32
    #include <windows.h>
    #include <Psapi.h>
#endif

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
        std::string generateMarker() 
        {
            std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
            std::uniform_int_distribution<int> dist(100000, 999999);
            return "kdotool_cpp_" + std::to_string(dist(rng));
        }

        std::string buildScript(const std::string& dbusAddr, const std::string& action)
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

        std::string kwinScriptGetResult(const std::string& marker, const std::string& action)
        {
            DBusError err;
            dbus_error_init(&err);

            // TWO separate connections like kdotool does:
            // self_conn = receives callbacks from KWin script
            // kwin_conn = sends commands to KWin
            DBusConnection* self_conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
            if (!self_conn || dbus_error_is_set(&err))
            {
                dbus_error_free(&err);
                return "";
            }

            // Use self_conn's unique name as the dbus addr (like kdotool's self_conn.unique_name())
            const char* uniqueName = dbus_bus_get_unique_name(self_conn);
            if (!uniqueName)
            {
                dbus_connection_close(self_conn);
                dbus_connection_unref(self_conn);
                return "";
            }
            std::string dbusAddr = uniqueName;

            DBusConnection* kwin_conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
            if (!kwin_conn || dbus_error_is_set(&err))
            {
                dbus_error_free(&err);
                dbus_connection_close(self_conn);
                dbus_connection_unref(self_conn);
                return "";
            }

            // Write script to temp file
            std::string script = buildScript(dbusAddr, action);
            std::string tmpPath = "/tmp/" + marker + ".js";
            {
                FILE* f = fopen(tmpPath.c_str(), "w");
                if (!f)
                {
                    dbus_connection_close(self_conn);
                    dbus_connection_unref(self_conn);
                    dbus_connection_close(kwin_conn);
                    dbus_connection_unref(kwin_conn);
                    return "";
                }
                fwrite(script.c_str(), 1, script.size(), f);
                fclose(f);
            }

            // loadScript via kwin_conn
            DBusMessage* msg = dbus_message_new_method_call(
                "org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "loadScript"
            );
            const char* pathCStr = tmpPath.c_str();
            const char* nameCStr = marker.c_str();
            dbus_message_append_args(msg,
                DBUS_TYPE_STRING, &pathCStr,
                DBUS_TYPE_STRING, &nameCStr,
                DBUS_TYPE_INVALID);

            DBusMessage* reply = dbus_connection_send_with_reply_and_block(kwin_conn, msg, 5000, &err);
            dbus_message_unref(msg);
            if (!reply || dbus_error_is_set(&err))
            {
                dbus_error_free(&err);
                std::remove(tmpPath.c_str());
                dbus_connection_close(self_conn);
                dbus_connection_unref(self_conn);
                dbus_connection_close(kwin_conn);
                dbus_connection_unref(kwin_conn);
                return "";
            }

            int scriptId = -1;
            dbus_message_get_args(reply, &err, DBUS_TYPE_INT32, &scriptId, DBUS_TYPE_INVALID);
            dbus_message_unref(reply);

            if (scriptId < 0)
            {
                std::remove(tmpPath.c_str());
                dbus_connection_close(self_conn);
                dbus_connection_unref(self_conn);
                dbus_connection_close(kwin_conn);
                dbus_connection_unref(kwin_conn);
                return "";
            }

            std::string scriptPath = "/Scripting/Script" + std::to_string(scriptId);

            // run() via kwin_conn
            msg = dbus_message_new_method_call("org.kde.KWin", scriptPath.c_str(), "org.kde.kwin.Script", "run");
            reply = dbus_connection_send_with_reply_and_block(kwin_conn, msg, 5000, &err);
            dbus_message_unref(msg);
            if (reply) dbus_message_unref(reply);

            // stop() via kwin_conn
            msg = dbus_message_new_method_call("org.kde.KWin", scriptPath.c_str(), "org.kde.kwin.Script", "stop");
            reply = dbus_connection_send_with_reply_and_block(kwin_conn, msg, 5000, &err);
            dbus_message_unref(msg);
            if (reply) dbus_message_unref(reply);

            // Poll for result on self_conn
            std::string result;
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            bool got_result = false;
            while (std::chrono::steady_clock::now() < deadline)
            {
                dbus_connection_read_write(self_conn, 100);
                DBusMessage* incoming = dbus_connection_pop_message(self_conn);
                if (!incoming) continue;

                const char* member = dbus_message_get_member(incoming);
                const char* iface  = dbus_message_get_interface(incoming);

                if (member && std::string(member) == "result")
                {
                    const char* val = nullptr;
                    dbus_message_get_args(incoming, &err, DBUS_TYPE_STRING, &val, DBUS_TYPE_INVALID);
                    if (val) result = val;
                    DBusMessage* replyMsg = dbus_message_new_method_return(incoming);
                    dbus_connection_send(self_conn, replyMsg, nullptr);
                    dbus_message_unref(replyMsg);
                    dbus_message_unref(incoming);
                    got_result = true;
                    break;
                }
                dbus_message_unref(incoming);
            }

            // unloadScript via kwin_conn
            std::string unloadName = marker;
            const char* unloadNameCStr = unloadName.c_str();
            msg = dbus_message_new_method_call("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "unloadScript");
            dbus_message_append_args(msg, DBUS_TYPE_STRING, &unloadNameCStr, DBUS_TYPE_INVALID);
            reply = dbus_connection_send_with_reply_and_block(kwin_conn, msg, 5000, &err);
            dbus_message_unref(msg);
            if (reply) dbus_message_unref(reply);

            std::remove(tmpPath.c_str());
            dbus_connection_close(self_conn);
            dbus_connection_unref(self_conn);
            dbus_connection_close(kwin_conn);
            dbus_connection_unref(kwin_conn);
            return result;
        }

        std::string kdeGetActiveWindowClass()
        {
            std::string marker = generateMarker();
            return kwinScriptGetResult(marker, "output_result(w.resourceClass);");
        }

        std::string kdeGetActiveWindowName()
        {
            std::string marker = generateMarker();
            return kwinScriptGetResult(marker, "output_result(w.caption);");
        }

        std::string kdeGetActiveWindowPid()
        {
            std::string marker = generateMarker();
            return kwinScriptGetResult(marker, "output_result(w.pid);");
        }
    }

#endif

static std::string s_qdbus_bin = "";

std::string getQDBusCommand() 
{
    if (!s_qdbus_bin.empty()) return s_qdbus_bin;
    
    std::string cmd = "command -v qdbus6 || command -v qdbus-qt6 || command -v qdbus";
    std::string check = runSystemCommand_UNSAFE(cmd);
    
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
                runSystemCommand_UNSAFE(extensionCheckCommand);

            Logger::log("[HPR] GNOME extension check: " + extensionCheckResult);

            bool extensionWorking =
                extensionCheckResult.contains("('");

            if (!extensionWorking && extensionFilesExist) 
            {
                Logger::log("[HPR] Enabling GNOME extension...");

                std::string cmd = "gnome-extensions enable "
                    "lol-another-window-extension@plexescor";
                runSystemCommand_UNSAFE(cmd);
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
                runSystemCommand_UNSAFE(extensionCheckCommand);

            return result.contains("('");
        },
        []() -> std::string
        {
            std::string command =
                "gdbus call --session --dest org.gnome.Shell --object-path "
                "/org/gnome/Shell/Extensions/LolAnotherWindowExtension --method "
                "org.gnome.Shell.Extensions.LolAnotherWindowExtension.FocusClass";

            std::string result = runSystemCommand_UNSAFE(command);

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

            std::string result = runSystemCommand_UNSAFE(command);

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
                "org.gnome.Shell.Extensions.LolAnotherWindowExtension.FocusAppId";

            std::string result = runSystemCommand_UNSAFE(command);

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

#ifdef __linux__
    registerBackend({
        "KDE",
        [](const std::string& env) { return env.contains("KDE"); },

        []() {}, 

        []() -> bool  // isUsable 
        {
            DBusError err;
            dbus_error_init(&err);
            DBusConnection* conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
            if (!conn || dbus_error_is_set(&err)) { dbus_error_free(&err); return false; }
            bool exists = dbus_bus_name_has_owner(conn, "org.kde.KWin", &err);
            dbus_error_free(&err);
            dbus_connection_close(conn);
            dbus_connection_unref(conn);
            return exists;

        },

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
        },

        []() -> std::string
        {
            return MotherfuckingKDE::kdeGetActiveWindowPid();
        }

    });
#endif

    registerBackend
    ({
        "Hyprland",

        [](const std::string& env) 
        {
            return env.contains("Hyprland");
        },

        []() {},

        []()
        {
            std::string cmd = "hyprctl monitors -j";
            std::string result = runSystemCommand_UNSAFE(cmd);
            // Logger::log("hyprctl monitors result: '" + result + "'");
            return result.contains("id");
        },

        []() -> std::string
        {
            std::string cmd = "hyprctl activewindow -j";
            std::string json = runSystemCommand_UNSAFE(cmd);
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
            std::string json = runSystemCommand_UNSAFE(cmd);
            const std::string key = "\"title\":";
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
            std::string json = runSystemCommand_UNSAFE(cmd);
            const std::string key = "\"pid\":";
            size_t keyPos = json.find(key);
            if (keyPos == std::string::npos) return "";
            size_t numStart = json.find_first_not_of(" \t", keyPos + key.size());
            if (numStart == std::string::npos) return "";
            size_t numEnd = json.find_first_of(",}\n", numStart);
            return json.substr(numStart, numEnd - numStart);
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
                return true;
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
        },

        []() -> std::string
        {
        #ifdef _WIN32
            HWND hwnd = GetForegroundWindow();
            if (!hwnd) return "";
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            return pid ? std::to_string(pid) : "";
        #endif
            return "";
        }

    });

    registerBackend
    ({
        "niri",

        [](const std::string& env)
        {
            return env.contains("niri");
        },

        []() {},

        []() -> bool
        {
            return true;
        },

        []() -> std::string
        {
            std::string cmd = "niri msg --json focused-window 2>/dev/null";
            std::string json = runSystemCommand_UNSAFE(cmd);
            const std::string key = "\"app_id\":";
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
            std::string cmd = "niri msg --json focused-window 2>/dev/null";
            std::string json = runSystemCommand_UNSAFE(cmd);
            const std::string key = "\"title\":";
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
            std::string cmd = "niri msg --json focused-window 2>/dev/null";
            std::string json = runSystemCommand_UNSAFE(cmd);
            const std::string key = "\"pid\":";
            size_t keyPos = json.find(key);
            if (keyPos == std::string::npos) return "";
            size_t numStart = json.find_first_not_of(" \t", keyPos + key.size());
            if (numStart == std::string::npos) return "";
            size_t numEnd = json.find_first_of(",}\n", numStart);
            return json.substr(numStart, numEnd - numStart);
        }
    });

    registerBackend
    ({
        "Cinnamon",

        [](const std::string& env) 
        {
            return env.contains("Cinnamon");
        },

        []() {},

        []() -> bool
        {
            std::string cmd = "gdbus call --session --dest org.Cinnamon --object-path /org/Cinnamon --method org.Cinnamon.Eval \"global.display.focus_window.get_wm_class()\"";
            std::string check = runSystemCommand_UNSAFE(cmd);
            return (check.contains("(true,") || check.contains("(false,"));
        },

        []() -> std::string
        {
            std::string cmd = "gdbus call --session --dest org.Cinnamon --object-path /org/Cinnamon --method org.Cinnamon.Eval \"global.display.focus_window.get_wm_class()\"";
            std::string raw = runSystemCommand_UNSAFE(cmd);

            // Format: (true, '"Value"')  or  ('Value',)
            // Try double-quote extraction first
            size_t dq1 = raw.find('"');
            size_t dq2 = raw.rfind('"');
            if (dq1 != std::string::npos && dq2 != std::string::npos && dq1 != dq2)
                return raw.substr(dq1 + 1, dq2 - dq1 - 1);

            // Fallback: single-quote extraction
            size_t sq1 = raw.find('\'');
            size_t sq2 = raw.rfind('\'');
            if (sq1 != std::string::npos && sq2 != std::string::npos && sq1 != sq2)
                return raw.substr(sq1 + 1, sq2 - sq1 - 1);

            return "";
        },

        []() -> std::string
        {
            std::string cmd = "gdbus call --session --dest org.Cinnamon --object-path /org/Cinnamon --method org.Cinnamon.Eval \"global.display.focus_window.get_title()\"";
            std::string raw = runSystemCommand_UNSAFE(cmd);

            size_t dq1 = raw.find('"');
            size_t dq2 = raw.rfind('"');
            if (dq1 != std::string::npos && dq2 != std::string::npos && dq1 != dq2)
                return raw.substr(dq1 + 1, dq2 - dq1 - 1);

            size_t sq1 = raw.find('\'');
            size_t sq2 = raw.rfind('\'');
            if (sq1 != std::string::npos && sq2 != std::string::npos && sq1 != sq2)
                return raw.substr(sq1 + 1, sq2 - sq1 - 1);

            return "";
        },

        []() -> std::string
        {
            std::string cmd = "gdbus call --session --dest org.Cinnamon --object-path /org/Cinnamon --method org.Cinnamon.Eval \"global.display.focus_window.get_pid()\"";
            std::string raw = runSystemCommand_UNSAFE(cmd);

            // Format: (true, 12345,) — grab number after the comma
            size_t comma = raw.find(',');
            if (comma == std::string::npos) return "";
            size_t numStart = raw.find_first_not_of(" \t", comma + 1);
            if (numStart == std::string::npos) return "";
            size_t numEnd = raw.find_first_of(",)\n", numStart);
            return raw.substr(numStart, numEnd - numStart);
        }
    });
}