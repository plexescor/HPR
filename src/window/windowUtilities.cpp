#include <cstdio> //For piping
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>

#ifdef __linux__
#include <sys/wait.h> // WEXITSTATUS
#elif defined(_WIN32)
#include "wintoastlib.h"
#endif

#include "windowUtilities.hpp"

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

        // Process killing
        "kill ",     "kill\t",     "killall ", "killall\t",
        "pkill ",    "pkill\t",

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
        "taskkill ", "taskkill\t", "wmic ",      "wmic\t",
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

    // Block dangerous commands on all platforms
    if (isCommandBlocked(command))
    {
        std::cout << "Haha motherfucker what did you expect 🤣🤣🤣" << std::endl;
        std::cerr << "[HPR] Blocked dangerous command: " << command << std::endl;
        return "Haha motherfucker what did you expect 🤣🤣🤣";
    }

#ifdef __linux__

    FILE *pipe = popen(command.c_str(), "r"); // Get only read access

    if (!pipe) {
      std::cerr << "Opening the pipe failed!\n";
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
    }

    // std::cout << "Output; " << output << std::endl;
    return output;

#elif defined(_WIN32)

    FILE *pipe = _popen(command.c_str(), "r");

    if (!pipe) {
      std::cerr << "Opening the pipe failed!\n";
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
    }

    // std::cout << "Output; " << output << std::endl;
    return output;

#elif defined(_WIN32)

    FILE *pipe = _popen(command.c_str(), "r");

    if (!pipe) {
      std::cerr << "Opening the pipe failed!\n";
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
    }

    return output;

#endif
    return "";
}

void showNotification(const std::string &title, const std::string &msg) {
#ifdef __linux__
    std::string escapedTitle = "";
    for (char c : title) {
        if (c == '"' || c == '\\' || c == '`' || c == '$') escapedTitle += '\\';
        escapedTitle += c;
    }
    std::string escapedMsg = "";
    for (char c : msg) {
        if (c == '"' || c == '\\' || c == '`' || c == '$') escapedMsg += '\\';
        escapedMsg += c;
    }
    std::string command = "notify-send \"" + escapedTitle + "\" \"" + escapedMsg + "\"";
    runSystemCommand(command);
#elif defined(_WIN32)
    static bool initialized = false;
    if (!initialized) {
        WinToastLib::WinToast::instance()->setAppName(L"HPR");
        WinToastLib::WinToast::instance()->setAppUserModelId(L"HPR.HumanPatternRecorder");
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
        
        WinToastLib::WinToast::instance()->showToast(templ, nullptr);
    }
#endif
}