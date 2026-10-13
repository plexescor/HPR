#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#else
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace
{

void printBanner()
{
    std::cout << "====================================================\n";
    std::cout << "            HPR Software Updater                   \n";
    std::cout << "====================================================\n\n";
}

void printErrorAndPause(const std::string &message)
{
    std::cerr << "\n[ERROR] " << message << "\n\n";
    std::cout << "Press [ENTER] to exit...";
    std::cin.get();
}

bool isElevated()
{
#ifdef _WIN32
    BOOL fRet = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize))
        {
            fRet = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return fRet != FALSE;
#else
    return geteuid() == 0;
#endif
}

bool canWriteToDirectory(const std::filesystem::path &dir)
{
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec))
    {
        auto parent = dir.parent_path();
        if (parent.empty()) parent = ".";
#ifdef _WIN32
        return std::filesystem::create_directories(dir, ec);
#else
        return access(parent.c_str(), W_OK) == 0;
#endif
    }

#ifdef _WIN32
    auto testFile = dir / ("__perm_test_" + std::to_string(GetCurrentProcessId()) + ".tmp");
    HANDLE hFile = CreateFileW(testFile.wstring().c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile);
        std::filesystem::remove(testFile, ec);
        return true;
    }
    return false;
#else
    return access(dir.c_str(), W_OK) == 0;
#endif
}

} // namespace

int main(int argc, char *argv[])
{
    printBanner();

    uint32_t targetPid = 0;
    std::filesystem::path targetDir;
    std::filesystem::path sourceDir;

    // Parse command line arguments: --pid <PID> --target-dir <DIR> --source-dir <SRC>
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--pid" && i + 1 < argc)
        {
            try
            {
                targetPid = static_cast<uint32_t>(std::stoul(argv[++i]));
            }
            catch (...)
            {
                printErrorAndPause("Invalid --pid argument provided.");
                return 1;
            }
        }
        else if (arg == "--target-dir" && i + 1 < argc)
        {
            targetDir = argv[++i];
        }
        else if (arg == "--source-dir" && i + 1 < argc)
        {
            sourceDir = argv[++i];
        }
    }

    if (targetDir.empty() || sourceDir.empty())
    {
        printErrorAndPause("Missing required arguments.\nUsage: HPRUpdater --pid <PID> --target-dir <DIR> --source-dir <SRC>");
        return 1;
    }

    // Verify write permissions and elevate if needed
    if (!canWriteToDirectory(targetDir) && !isElevated())
    {
        std::cout << "[Updater] Installation directory requires administrative privileges. Requesting elevation...\n";
#ifdef _WIN32
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);

        std::wstring allArgs;
        for (int i = 1; i < argc; ++i)
        {
            if (i > 1) allArgs += L" ";
            std::string s = argv[i];
            if (s.find(' ') != std::string::npos)
            {
                allArgs += L"\"" + std::wstring(s.begin(), s.end()) + L"\"";
            }
            else
            {
                allArgs += std::wstring(s.begin(), s.end());
            }
        }

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = exePath;
        sei.lpParameters = allArgs.c_str();
        sei.nShow = SW_SHOW;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;

        if (ShellExecuteExW(&sei))
        {
            return 0;
        }
        else
        {
            printErrorAndPause("Administrator elevation was denied. Cannot update HPR in this folder.");
            return 1;
        }
#else
        std::vector<char*> newArgs;
        newArgs.push_back(const_cast<char*>("sudo"));
        for (int i = 0; i < argc; ++i)
        {
            newArgs.push_back(argv[i]);
        }
        newArgs.push_back(nullptr);
        execvp("sudo", newArgs.data());
        printErrorAndPause("Failed to elevate with sudo. Please run the updater as root or with sudo.");
        return 1;
#endif
    }

    std::cout << "Target Directory : " << targetDir.string() << "\n";
    std::cout << "Source Directory : " << sourceDir.string() << "\n";
    std::cout << "Privilege Level  : " << (isElevated() ? "Administrator / Root" : "Standard User") << "\n";
    if (targetPid > 0)
    {
        std::cout << "Waiting for PID  : " << targetPid << "\n";
    }
    std::cout << "----------------------------------------------------\n\n";

    // Step 1: Wait for running HPR process to exit
    if (targetPid > 0)
    {
        std::cout << "[1/4] Waiting for HPR process to terminate...\n";
#ifdef _WIN32
        HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, targetPid);
        if (hProcess != NULL)
        {
            WaitForSingleObject(hProcess, INFINITE);
            CloseHandle(hProcess);
        }
#else
        while (kill(static_cast<pid_t>(targetPid), 0) == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
#endif
        // Grace period to ensure OS releases all file handles
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "      HPR has shut down cleanly.\n\n";
    }
    else
    {
        std::cout << "[1/4] No active PID specified; proceeding immediately.\n\n";
    }

    // Step 2: Validate source files
    std::cout << "[2/4] Validating replacement files...\n";
    if (!std::filesystem::exists(sourceDir))
    {
        printErrorAndPause("Source directory does not exist: " + sourceDir.string());
        return 1;
    }

#ifdef _WIN32
    std::filesystem::path mainBinary = sourceDir / "HPR.exe";
#else
    std::filesystem::path mainBinary = sourceDir / "HPR";
#endif

    if (!std::filesystem::exists(mainBinary))
    {
        printErrorAndPause("Could not find new HPR executable in source directory: " + mainBinary.string());
        return 1;
    }
    std::cout << "      Found: " << mainBinary.filename().string() << "\n\n";

    // Step 3: Copy update files to target directory
    std::cout << "[3/4] Copying updated files to installation directory...\n";
    try
    {
        std::filesystem::create_directories(targetDir);

        for (const auto &entry : std::filesystem::directory_iterator(sourceDir))
        {
            std::filesystem::path dest = targetDir / entry.path().filename();
            std::cout << "      Installing: " << entry.path().filename().string() << "\n";

            std::error_code ec;
            std::filesystem::copy(
                entry.path(), dest,
                std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive,
                ec);

            if (ec)
            {
                printErrorAndPause("Failed to copy " + entry.path().filename().string() + ": " + ec.message());
                return 1;
            }

#ifndef _WIN32
            // On Linux, make sure HPR binary is marked executable
            if (entry.path().filename() == "HPR")
            {
                std::filesystem::permissions(
                    dest,
                    std::filesystem::perms::owner_all | std::filesystem::perms::group_read |
                    std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
                    std::filesystem::perms::others_exec,
                    std::filesystem::perm_options::replace, ec);
            }
#endif
        }
        std::cout << "      All files copied successfully.\n\n";
    }
    catch (const std::exception &e)
    {
        printErrorAndPause("Error during file copy: " + std::string(e.what()));
        return 1;
    }

    // Step 4: Relaunch updated HPR application
    std::cout << "[4/4] Launching updated HPR...\n";
#ifdef _WIN32
    std::filesystem::path hprPath = targetDir / "HPR.exe";
    std::wstring wExe = hprPath.wstring();
    std::wstring wDir = targetDir.wstring();

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    if (!CreateProcessW(wExe.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, wDir.c_str(), &si, &pi))
    {
        printErrorAndPause("Failed to relaunch HPR.exe. Please start HPR manually.");
        return 1;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    std::filesystem::path hprPath = targetDir / "HPR";
    pid_t pid = fork();
    if (pid == 0)
    {
        chdir(targetDir.string().c_str());
        char *args[] = { (char*)hprPath.c_str(), nullptr };
        execv(hprPath.c_str(), args);
        _exit(1);
    }
    else if (pid < 0)
    {
        printErrorAndPause("Failed to fork and relaunch HPR. Please start HPR manually.");
        return 1;
    }
#endif

    std::cout << "      HPR has been launched successfully!\n\n";
    std::cout << "Update complete. Closing updater in 2 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    return 0;
}
