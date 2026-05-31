#include "windowBackendRegistery.hpp"
#include "windowUtilities.hpp"
#include <string>
#include <iostream>
#include <filesystem>
#ifdef _WIN32
    #include <windows.h>
    #include <Psapi.h>
#endif
#ifdef __linux__
    #include <unistd.h>
    #include <dirent.h>
    #include <sys/stat.h>
#endif

static std::string runHyprctl(const std::string& args) 
{
    std::string sig;
    if (const char* e = getenv("HYPRLAND_INSTANCE_SIGNATURE")) 
    {
        sig = e;
    }
    else 
    {
        std::vector<std::string> dirs;
        if (const char* xdg = getenv("XDG_RUNTIME_DIR")) 
            dirs.push_back(std::string(xdg) + "/hypr");
        dirs.push_back("/tmp/hypr");

        for (const auto& dir : dirs)
        {
            DIR* d = opendir(dir.c_str());
            if (!d) continue;
            dirent* entry;
            while ((entry = readdir(d)))
            {
                std::string name = entry->d_name;
                if (name.empty() || name[0] == '.') continue;
                if (name.size() >= 5 && name.substr(name.size() - 5) == ".lock") continue;
                // make sure it's a directory
                std::string fullPath = dir + "/" + name;
                struct stat st;
                if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
                {
                    sig = name;
                    break;
                }
            }
            closedir(d);
            if (!sig.empty()) break;
        }
    }

    std::string cmd = (sig.empty() ? "" : "HYPRLAND_INSTANCE_SIGNATURE=" + sig + " ")
                    + "/usr/bin/hyprctl " + args;
    return runSystemCommand(cmd);
}
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
                // std::cout << "[KDE] Failed to get self_conn: " << (dbus_error_is_set(&err) ? err.message : "null") << std::endl;
                dbus_error_free(&err);
                return "";
            }

            // Use self_conn's unique name as the dbus addr (like kdotool's self_conn.unique_name())
            const char* uniqueName = dbus_bus_get_unique_name(self_conn);
            if (!uniqueName)
            {
                // std::cout << "[KDE] Failed to get unique name" << std::endl;
                dbus_connection_close(self_conn);
                dbus_connection_unref(self_conn);
                return "";
            }
            std::string dbusAddr = uniqueName;
            // std::cout << "[KDE] self_conn unique name: " << dbusAddr << std::endl;

            DBusConnection* kwin_conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
            if (!kwin_conn || dbus_error_is_set(&err))
            {
                // std::cout << "[KDE] Failed to get kwin_conn: " << (dbus_error_is_set(&err) ? err.message : "null") << std::endl;
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
                    // std::cout << "[KDE] Failed to write temp file" << std::endl;
                    dbus_connection_close(self_conn);
                    dbus_connection_unref(self_conn);
                    dbus_connection_close(kwin_conn);
                    dbus_connection_unref(kwin_conn);
                    return "";
                }
                fwrite(script.c_str(), 1, script.size(), f);
                fclose(f);
            }
            // std::cout << "[KDE] script written to: " << tmpPath << std::endl;

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
                // std::cout << "[KDE] loadScript failed: " << (dbus_error_is_set(&err) ? err.message : "no reply") << std::endl;
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
            // std::cout << "[KDE] scriptId: " << scriptId << std::endl;

            if (scriptId < 0)
            {
                // std::cout << "[KDE] KWin rejected script (scriptId < 0)" << std::endl;
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
            // if (!reply || dbus_error_is_set(&err))
                // std::cout << "[KDE] run() failed: " << (dbus_error_is_set(&err) ? err.message : "no reply") << std::endl;
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
                // std::cout << "[KDE] incoming - member: " << (member ? member : "null")
                        // << " iface: " << (iface ? iface : "null") << std::endl;

                if (member && std::string(member) == "result")
                {
                    const char* val = nullptr;
                    dbus_message_get_args(incoming, &err, DBUS_TYPE_STRING, &val, DBUS_TYPE_INVALID);
                    if (val) result = val;
                    // std::cout << "[KDE] got result: " << result << std::endl;
                    DBusMessage* replyMsg = dbus_message_new_method_return(incoming);
                    dbus_connection_send(self_conn, replyMsg, nullptr);
                    dbus_message_unref(replyMsg);
                    dbus_message_unref(incoming);
                    got_result = true;
                    break;
                }
                dbus_message_unref(incoming);
            }

            // if (!got_result)
                // std::cout << "[KDE] timed out waiting for result" << std::endl;

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
            return runHyprctl("-j activewindow").contains("class");
        },

        []() -> std::string
        {
            std::string json = runHyprctl("activewindow -j");
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
            std::string json = runHyprctl("activewindow -j");
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