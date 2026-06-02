#include <cstdio> //For piping
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <fstream>
#ifdef __linux__
    #include <dbus/dbus.h>
#endif
#include "logger.hpp"

#ifdef __linux__
#include <sys/wait.h> // WEXITSTATUS
#include <unistd.h>
#elif defined(_WIN32)
#include "wintoastlib.h"
#include <shellapi.h>
#endif

#include "windowUtilities.hpp"
#include "appState.hpp"
#include "extensionManager.hpp"

namespace
{
    // Commands that should never be executed from extensions or internally.
    // Checked against the lowercased command string.
    const std::vector<std::string> blockedCommands = {
        // Destructive filesystem operations
        "rm ",    "rm\t",    "rmdir ",   "rmdir\t",
        "shred ", "shred\t", "unlink ",  "unlink\t",
        "truncate ", "truncate\t",

        // Permission / ownership manipulation
        "chmod ",  "chmod\t",  "chown ",  "chown\t",
        "chgrp ",  "chgrp\t",  "chattr ", "chattr\t",

        // Privilege escalation
        "sudo ",  "sudo\t",  "su ",      "su\t",
        "doas ",  "doas\t",  "pkexec ",  "pkexec\t",
        "runas ", "runas\t",

        // Disk/partition manipulation
        "mkfs ",   "mkfs.",   "fdisk ",  "dd ",    "dd\t",
        "format ", "format\t", "parted ", "parted\t",
        "mkswap ", "mkswap\t", "wipefs ", "wipefs\t",
        "lvm ",    "lvm\t",

        // System shutdown/reboot
        "shutdown ", "shutdown\t", "reboot ", "reboot\t",
        "poweroff ", "poweroff\t", "halt ",   "halt\t",
        "init 0",    "init 6",

        // Network exfiltration / download
        "curl ",   "curl\t",   "wget ",   "wget\t",
        "nc ",     "nc\t",     "ncat ",   "ncat\t",
        "netcat ", "netcat\t", "socat ",  "socat\t",
        "scp ",    "scp\t",    "rsync ",  "rsync\t",
        "sftp ",   "sftp\t",   "ftp ",    "ftp\t",
        "ssh ",    "ssh\t",

        // Code execution / interpreters (prevent running arbitrary scripts)
        "python ", "python\t", "python3 ", "python3\t",
        "node ",   "node\t",   "perl ",    "perl\t",
        "ruby ",   "ruby\t",   "bash ",    "bash\t",
        "sh ",     "sh\t",     "zsh ",     "zsh\t",
        "fish ",   "fish\t",   "csh ",     "csh\t",
        "cmd ",    "cmd\t",    "cmd.exe",
        "powershell", "pwsh",
        "eval ",   "eval\t",   "exec ",    "exec\t",

        // Cron / scheduled tasks
        "crontab ", "crontab\t", "at ", "at\t",

        // Firewall / networking config
        "iptables ",  "iptables\t",  "ip6tables ", "ip6tables\t",
        "nft ",       "nft\t",       "ufw ",       "ufw\t",
        "firewall-cmd ",

        // Service / systemd manipulation
        "systemctl ", "systemctl\t", "service ", "service\t",
        "journalctl ", "journalctl\t",

        // Mount / filesystem
        "mount ",   "mount\t",   "umount ",  "umount\t",
        "losetup ", "losetup\t", "fsck ",    "fsck\t",

        // Windows-specific dangerous commands
        "del ",      "del\t",      "erase ",     "erase\t",
        "rd ",       "rd\t",       "reg ",       "reg\t",
        "wmic ",      "wmic\t",
        "bcdedit ",  "bcdedit\t",  "diskpart",
        "schtasks ", "schtasks\t", "icacls ",    "icacls\t",
        "takeown ",  "takeown\t",  "cipher ",    "cipher\t",
        "net user",  "net stop",   "net start",  "netsh ",
        "sc delete", "sc stop",    "sc config",

        // Package management (prevent installing/removing software)
        "apt ",    "apt-get ", "dnf ",   "yum ",
        "pacman ", "zypper ",  "snap ",  "flatpak ",
        "pip ",    "pip3 ",    "npm ",   "cargo ",
        "brew ",   "brew\t",   "choco ", "choco\t",
        "winget ", "winget\t", "scoop ", "scoop\t",

        // Dangerous redirections / overwrites
        "> /dev/",   ">/dev/",
        "> /etc/",   ">/etc/",
        "> /proc/",  ">/proc/",
        "> /sys/",   ">/sys/",
        "| tee /etc", "| tee /dev", "| tee /sys",
        "mv ",       "mv\t",  // prevent moving/renaming system files
        "cp /dev/",  // prevent copying from raw devices

        // Chaining/piping to bypass (catch common shell injection patterns)
        "| rm",    "| sudo",   "; rm",    "; sudo",
        "&& rm",   "&& sudo",  "|| rm",   "|| sudo",
        "$(rm",    "$(sudo",   "`rm",     "`sudo",
        "; sh",    "| sh",     "&& sh",   "|| sh",
        "; bash",  "| bash",   "&& bash", "|| bash",
        "; curl",  "| curl",   "&& curl", "|| curl",
        "; wget",  "| wget",   "&& wget", "|| wget",
        "| dd ",   "; dd ",    "&& dd ",  "|| dd ",

        // Encoding tricks to bypass filters
        "base64 -d", "base64 --decode",
        "xxd ",      "xxd\t",
    };

    bool isCommandBlocked(const std::string& command)
    {
        // Lowercase the command for case-insensitive matching
        std::string lower = command;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return std::tolower(c); });

        for (const auto& blocked : blockedCommands)
        {
            // Check if the command starts with or contains the blocked pattern
            if (lower.find(blocked) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }
}

std::string runSystemCommand(std::string &command) {

    if (isCommandBlocked(command))
    {
        std::cout << "Haha motherfucker what did you expect 🤣🤣🤣" << std::endl;
        std::cerr << "[HPR] Blocked dangerous command: " << command << std::endl;
        Logger::log("[HPR] Blocked dangerous command: " + command);
        return "Haha motherfucker what did you expect 🤣🤣🤣";
    }

#ifdef __linux__

    int pipefd[2];
    if (pipe(pipefd) < 0) return "";

    pid_t pid = fork();
    if (pid < 0) return "";

    if (pid == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(1);
    }

    close(pipefd[1]);
    std::string output;
    char buffer[256];
    ssize_t n;
    while ((n = read(pipefd[0], buffer, sizeof(buffer))) > 0)
        output.append(buffer, n);
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
        std::cerr << "Running the command failed! Exit Code: " << WEXITSTATUS(status) << std::endl;
        Logger::log("[HPR] Running the command failed! Exit Code: " + std::to_string(WEXITSTATUS(status)));

    return output;
#elif defined(_WIN32)

    FILE *pipe = _popen(command.c_str(), "r");

    if (!pipe) {
      std::cerr << "Opening the pipe failed!\n";
      Logger::log("[HPR] Opening the pipe failed for command: " + command);
      return "";
    }

    std::string output = "";
    char buffer[256];

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      output += buffer;
    }

    int exitCode = _pclose(pipe);

    if (exitCode != 0) {
      std::cerr << "Running the command failed! Exit Code: " << exitCode << std::endl;
      Logger::log("[HPR] Running the command failed! Exit Code: " + std::to_string(exitCode) + " for command: " + command);
    }

    return output;

#endif
    return "";
}

std::string runSystemCommand_UNSAFE(std::string &command) {

#ifdef __linux__

    FILE *pipe = popen(command.c_str(), "r"); // Get only read access

    if (!pipe) {
      std::cerr << "Opening the pipe failed!\n";
      Logger::log("[HPR] Opening the pipe failed for command: " + command);
      return "";
    }

    std::string output = "";
    char buffer[256]; // 255 normal + 1 nullterm

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      output += buffer;
    }

    int exitCode = pclose(pipe);

    if (exitCode != 0) {
      std::cerr << "Running the command failed! Exit Code: " << WEXITSTATUS(exitCode) << std::endl;
      Logger::log("[HPR] Running the command failed! Exit Code: " + std::to_string(WEXITSTATUS(exitCode)) + " for command: " + command);
    }
    return output;

#elif defined(_WIN32)

    FILE *pipe = _popen(command.c_str(), "r");

    if (!pipe) {
      std::cerr << "Opening the pipe failed!\n";
      Logger::log("[HPR] Opening the pipe failed for command: " + command);
      return "";
    }

    std::string output = "";
    char buffer[256];

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      output += buffer;
    }

    int exitCode = _pclose(pipe);

    if (exitCode != 0) {
      std::cerr << "Running the command failed! Exit Code: " << exitCode << std::endl;
      Logger::log("[HPR] Running the command failed! Exit Code: " + std::to_string(exitCode) + " for command: " + command);
    }

    return output;

#endif
    return "";
}

#ifdef _WIN32
class DummyWinToastHandler : public WinToastLib::IWinToastHandler {
public:
    void toastActivated() const override {}
    void toastActivated(int actionIndex) const override {}
    void toastActivated(std::wstring response) const override {}
    void toastDismissed(WinToastDismissalReason state) const override {}
    void toastFailed() const override {}
};
#endif

void showNotification(const std::string &title, const std::string &msg) {
    if (AppState::extManager)
    {
        auto res = AppState::extManager->dispatchOverride("showNotification", { CppValue(CppValue::Type::String, title), CppValue(CppValue::Type::String, msg) });
        if (res.has_value())
        {
            return;
        }
    }
#ifdef __linux__
    DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, nullptr);
    if (!conn) return;

    DBusMessage* message = dbus_message_new_method_call(
        "org.freedesktop.Notifications",
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        "Notify"
    );
    if (!message) { dbus_connection_unref(conn); return; }

    const char* app_name = "HPR";
    uint32_t replaces_id = 0;
    const char* app_icon = "";
    const char* summary = title.c_str();
    const char* body = msg.c_str();
    int32_t timeout = 5000;

    DBusMessageIter args, actions_iter, hints_iter;
    dbus_message_iter_init_append(message, &args);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &app_name);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &replaces_id);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &app_icon);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &summary);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &body);

    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "s", &actions_iter);
    dbus_message_iter_close_container(&args, &actions_iter);

    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &hints_iter);
    dbus_message_iter_close_container(&args, &hints_iter);

    dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &timeout);

    dbus_connection_send(conn, message, nullptr);
    dbus_connection_flush(conn);
    dbus_message_unref(message);
    dbus_connection_unref(conn);

#elif defined(_WIN32)
    thread_local static bool initialized = false;
    if (!initialized) {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        WinToastLib::WinToast::instance()->setAppName(L"HPR");
        WinToastLib::WinToast::instance()->setAppUserModelId(L"HPR - Human Pattern Recorder");
        WinToastLib::WinToast::instance()->setShortcutPolicy(WinToastLib::WinToast::SHORTCUT_POLICY_IGNORE);
        
        if (WinToastLib::WinToast::instance()->initialize()) {
            initialized = true;
        }
    }
    if (initialized) {
        std::wstring wtitle(title.begin(), title.end());
        std::wstring wmsg(msg.begin(), msg.end());
        
        WinToastLib::WinToastTemplate templ(WinToastLib::WinToastTemplate::ImageAndText02);
        templ.setTextField(wtitle, WinToastLib::WinToastTemplate::FirstLine);
        templ.setTextField(wmsg, WinToastLib::WinToastTemplate::SecondLine);
        
        const char* appData = std::getenv("APPDATA");
        if (appData) {
            std::string iconPath = std::string(appData) + "/HPR/HPR_Config/assets/logo_256png.png";
            std::wstring wIconPath(iconPath.begin(), iconPath.end());
            templ.setImagePath(wIconPath);
        }
        
        WinToastLib::WinToast::instance()->showToast(templ, new DummyWinToastHandler());
     }
#endif
}

#include <chrono>

#include <sstream>

#ifdef _WIN32
    #include <psapi.h>
    #include <tlhelp32.h>
#elif defined(__APPLE__)
    #include <mach/mach.h>
    #include <sys/sysctl.h>
#endif

std::string getCpuUsage() {
#ifdef __linux__
    static auto lastTime = std::chrono::steady_clock::now();
    static long lastUtime = 0;
    static long lastStime = 0;

    std::ifstream statFile("/proc/self/stat");
    if (!statFile.is_open()) return "0.00%";

    std::string line;
    if (!std::getline(statFile, line)) return "0.00%";

    size_t lastCloseParen = line.rfind(')');
    if (lastCloseParen == std::string::npos || lastCloseParen + 2 >= line.size()) return "0.00%";

    std::string rest = line.substr(lastCloseParen + 2);
    std::istringstream iss(rest);

    std::string state;
    long ppid, pgrp, session, tty_nr, tpgid;
    unsigned long flags, minflt, cminflt, majflt, cmajflt;
    unsigned long utime = 0, stime = 0;

    if (!(iss >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt >> utime >> stime)) {
        return "0.00%";
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - lastTime).count();
    
    long dUtime = utime - lastUtime;
    long dStime = stime - lastStime;

    lastTime = now;
    lastUtime = utime;
    lastStime = stime;

    long clockTicks = sysconf(_SC_CLK_TCK);
    long numCores = sysconf(_SC_NPROCESSORS_ONLN);
    if (elapsed == 0 || clockTicks <= 0 || numCores <= 0) return "0.00%";

    double usage = (double(dUtime + dStime) / double(clockTicks)) / (double(elapsed) / 1000000.0) * 100.0;
    usage = usage / double(numCores);
    if (usage < 0.0) usage = 0.0;

    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f%%", usage);
    return std::string(buf);
#elif defined(_WIN32)
    static ULONGLONG lastTime = 0;
    static ULONGLONG lastSystemTime = 0;

    FILETIME ftCreation, ftExit, ftKernel, ftUser;
    if (!GetProcessTimes(GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser)) {
        return "0.00%";
    }

    ULARGE_INTEGER kernel, user;
    kernel.LowPart = ftKernel.dwLowDateTime;
    kernel.HighPart = ftKernel.dwHighDateTime;
    user.LowPart = ftUser.dwLowDateTime;
    user.HighPart = ftUser.dwHighDateTime;

    ULONGLONG nowTime = GetTickCount64();
    ULONGLONG systemTime = kernel.QuadPart + user.QuadPart;

    if (lastTime == 0) {
        lastTime = nowTime;
        lastSystemTime = systemTime;
        return "0.00%";
    }

    ULONGLONG elapsed = nowTime - lastTime;
    ULONGLONG elapsedSystem = systemTime - lastSystemTime;

    lastTime = nowTime;
    lastSystemTime = systemTime;

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD numCores = sysInfo.dwNumberOfProcessors;

    if (elapsed == 0 || numCores == 0) return "0.00%";

    double usage = (double(elapsedSystem) / 10000.0) / double(elapsed) * 100.0;
    usage = usage / double(numCores);
    if (usage < 0.0) usage = 0.0;

    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f%%", usage);
    return std::string(buf);
#elif defined(__APPLE__)
    static auto lastTime = std::chrono::steady_clock::now();
    static double lastCpuTime = 0.0;

    task_thread_times_info_data_t threadTimes;
    mach_msg_type_number_t threadTimesCount = TASK_THREAD_TIMES_INFO_COUNT;
    kern_return_t kr = task_info(mach_task_self(), TASK_THREAD_TIMES_INFO, (task_info_t)&threadTimes, &threadTimesCount);
    if (kr != KERN_SUCCESS) return "0.00%";

    double cpuTime = double(threadTimes.user_time.seconds) + double(threadTimes.user_time.microseconds) / 1000000.0 +
                     double(threadTimes.system_time.seconds) + double(threadTimes.system_time.microseconds) / 1000000.0;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - lastTime).count();

    double dCpuTime = cpuTime - lastCpuTime;

    lastTime = now;
    lastCpuTime = cpuTime;

    int mib[2] = {CTL_HW, HW_NCPU};
    int numCores = 0;
    size_t len = sizeof(numCores);
    sysctl(mib, 2, &numCores, &len, NULL, 0);

    if (elapsed == 0 || numCores <= 0) return "0.00%";

    double usage = (dCpuTime / (double(elapsed) / 1000000.0)) * 100.0;
    usage = usage / double(numCores);
    if (usage < 0.0) usage = 0.0;

    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f%%", usage);
    return std::string(buf);
#else
    return "0.00%";
#endif
}

std::string getRamUsage() {
#ifdef __linux__
    std::ifstream statusFile("/proc/self/status");
    if (!statusFile.is_open()) return "0.0 MB";

    std::string line;
    while (std::getline(statusFile, line)) {
        if (line.rfind("RssAnon:", 0) == 0) {
            long kb = 0;
            sscanf(line.c_str(), "RssAnon: %ld", &kb);
            double mb = double(kb) / 1024.0;
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f MB", mb);
            return std::string(buf);
        }
    }
    return "0.0 MB";
#elif defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        double mb = double(pmc.PagefileUsage) / (1024.0 * 1024.0);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f MB", mb);
        return std::string(buf);
    }
    return "0.0 MB";
#elif defined(__APPLE__)
    task_vm_info_data_t vmInfo;
    mach_msg_type_number_t infoCount = TASK_VM_INFO_COUNT;
    kern_return_t kr = task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&vmInfo, &infoCount);
    if (kr != KERN_SUCCESS) return "0.0 MB";

    double mb = double(vmInfo.phys_footprint) / (1024.0 * 1024.0);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f MB", mb);
    return std::string(buf);
#else
    return "0.0 MB";
#endif
}

std::string getThreadCount() {
#ifdef __linux__
    std::ifstream statusFile("/proc/self/status");
    if (!statusFile.is_open()) return "0";

    std::string line;
    while (std::getline(statusFile, line)) {
        if (line.rfind("Threads:", 0) == 0) {
            long threads = 0;
            sscanf(line.c_str(), "Threads: %ld", &threads);
            return std::to_string(threads);
        }
    }
    return "0";
#elif defined(_WIN32)
    DWORD pid = GetCurrentProcessId();
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
    if (h == INVALID_HANDLE_VALUE) return "0";

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    if (Process32First(h, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                CloseHandle(h);
                return std::to_string(pe.cntThreads);
            }
        } while (Process32Next(h, &pe));
    }
    CloseHandle(h);
    return "0";
#elif defined(__APPLE__)
    thread_act_array_t threadList;
    mach_msg_type_number_t threadCount = 0;
    kern_return_t kr = task_threads(mach_task_self(), &threadList, &threadCount);
    if (kr != KERN_SUCCESS) return "0";

    for (mach_msg_type_number_t i = 0; i < threadCount; ++i) {
        mach_port_deallocate(mach_task_self(), threadList[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)threadList, sizeof(thread_t) * threadCount);

    return std::to_string(threadCount);
#else
    return "0";
#endif
}