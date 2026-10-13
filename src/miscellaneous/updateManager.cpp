#include "updateManager.hpp"
#include "appState.hpp"
#include "extensionManager.hpp"
#include "logger.hpp"
#include "netUtilities.hpp"
#include "singleInstance.hpp"

#include "miniz.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#else
#include <curl/curl.h>
#include <unistd.h>
#endif

// Slint event loop
#include <slint.h>

UpdateManager::UpdateManager()
{
}

UpdateManager::~UpdateManager()
{
    stop();
}

void UpdateManager::run()
{
    if (running)
    {
        return;
    }
    running = true;
    workerThread = std::thread(&UpdateManager::workerLoop, this);
}

void UpdateManager::stop()
{
    if (!running)
    {
        return;
    }
    running = false;
    if (workerThread.joinable())
    {
        workerThread.join();
    }
}

void UpdateManager::workerLoop()
{
    // Initial check on boot
    checkForUpdates();

    while (running)
    {
        // 3600 seconds interval, split into 100ms chunks to allow fast shutdown
        int remainingMs = 3600 * 1000;
        while (running && remainingMs > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            remainingMs -= 100;
        }

        if (!running)
        {
            break;
        }

        checkForUpdates();
    }
}

void UpdateManager::checkForUpdates()
{
    std::lock_guard<std::mutex> lock(updateMutex);

#ifndef NDEBUG
    // Debug mode local zip override check
    std::string debugZipPath = AppState::configManager.getConfig<std::string>("debug-update-zip-path", "");
    if (!debugZipPath.empty() && std::filesystem::exists(debugZipPath))
    {
        std::lock_guard<std::recursive_mutex> stateLock(AppState::stateMutex);
        AppState::state.updateAvailable = true;
        AppState::state.latestVersion = "Debug-Local";
        AppState::state.updateDownloadUrl = debugZipPath;
        Logger::log("[UpdateManager] Debug mode active. Local zip override found: " + debugZipPath);
        return;
    }
#endif

    // Verify configuration permissions
    if (!AppState::configManager.getConfig<bool>("allow-update-checking", true))
    {
        return;
    }
    if (!AppState::configManager.getConfig<bool>("allow-network-activity", true))
    {
        return;
    }

    std::map<std::string, std::string> headers = {
        {"User-Agent", "HPR-App"},
        {"Accept", "application/vnd.github.v3+json"}
    };

    auto [body, status] = NativeNet::httpGet("api.github.com", "/repos/plexescor/HPR/releases/latest", true, headers);
    if (status != 200 || body.empty())
    {
        return;
    }

    // Extract tag_name from JSON
    std::string tagKey = "\"tag_name\":";
    auto tagPos = body.find(tagKey);
    if (tagPos == std::string::npos)
    {
        return;
    }

    auto startQuote = body.find('"', tagPos + tagKey.length());
    if (startQuote == std::string::npos)
    {
        return;
    }
    auto endQuote = body.find('"', startQuote + 1);
    if (endQuote == std::string::npos)
    {
        return;
    }

    std::string rawTag = body.substr(startQuote + 1, endQuote - startQuote - 1);
    std::string normTag = rawTag;
    if (!normTag.empty() && (normTag[0] == 'v' || normTag[0] == 'V'))
    {
        normTag = normTag.substr(1);
    }

    if (normTag.empty())
    {
        return;
    }

    // Extract asset or zipball download url
    std::string downloadUrl = "";
    std::string zipballKey = "\"zipball_url\":";
    auto zipPos = body.find(zipballKey);
    if (zipPos != std::string::npos)
    {
        auto zStart = body.find('"', zipPos + zipballKey.length());
        if (zStart != std::string::npos)
        {
            auto zEnd = body.find('"', zStart + 1);
            if (zEnd != std::string::npos)
            {
                downloadUrl = body.substr(zStart + 1, zEnd - zStart - 1);
            }
        }
    }

    // Check if new version differs from current version
    if (normTag != AppState::APP_VERSION)
    {
        std::lock_guard<std::recursive_mutex> stateLock(AppState::stateMutex);
        AppState::state.updateAvailable = true;
        AppState::state.latestVersion = normTag;
        AppState::state.updateDownloadUrl = downloadUrl;
        Logger::log("[UpdateManager] Confirmed new release: " + normTag + " (current: " + AppState::APP_VERSION + ")");
    }
}

void UpdateManager::promptUserForUpdate()
{
    if (isUpdating.load())
    {
        return;
    }

    std::string versionStr;
    {
        std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
        versionStr = AppState::state.latestVersion;
    }

    std::string text = "A new version of HPR (" + versionStr + ") is available!\n\n"
                       "HPR will download and extract the update. An updater console will open, "
                       "and HPR will close to apply the update.\n\n"
                       "Do you want to proceed?";

    auto mgr = AppState::extManager;
    if (!mgr)
    {
        return;
    }

    mgr->showUiPopup(text, "CANCEL", "UPDATE & RESTART", false, [this](int btn) {
        if (btn == 1)
        {
            startUpdatePipeline();
        }
    });
}

void UpdateManager::startUpdatePipeline()
{
    if (isUpdating.exchange(true))
    {
        return;
    }

    {
        std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
        AppState::state.isUpdating = true;
        AppState::state.updateStatusText = "PREPARING UPDATE...";
    }

    std::thread([this]() {
        Logger::log("[UpdateManager] Starting update staging pipeline...");

        auto failUpdate = [this](const std::string &msg) {
            Logger::log("[UpdateManager] " + msg);
            {
                std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
                AppState::state.updateStatusText = "UPDATE FAILED";
            }
            std::this_thread::sleep_for(std::chrono::seconds(4));
            {
                std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
                AppState::state.isUpdating = false;
                AppState::state.updateStatusText = "Update HPR";
            }
            isUpdating = false;
        };

        std::string versionStr;
        std::string downloadUrl;
        {
            std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
            versionStr = AppState::state.latestVersion;
            downloadUrl = AppState::state.updateDownloadUrl;
        }

        std::filesystem::path tempBase = std::filesystem::temp_directory_path() / "hpr-update";
        try
        {
            std::filesystem::create_directories(tempBase);
        }
        catch (const std::exception &e)
        {
            failUpdate("Failed to create temp staging directory: " + std::string(e.what()));
            return;
        }

        std::filesystem::path zipFile = tempBase / "update.zip";
        std::filesystem::path extractDir = tempBase / "extracted";

        bool hasZip = false;

#ifndef NDEBUG
        std::string debugZipPath = AppState::configManager.getConfig<std::string>("debug-update-zip-path", "");
        if (!debugZipPath.empty() && std::filesystem::exists(debugZipPath))
        {
            zipFile = debugZipPath;
            hasZip = true;
            Logger::log("[UpdateManager] Using debug local zip: " + debugZipPath);
        }
#endif

        if (!hasZip)
        {
            if (downloadUrl.empty())
            {
                failUpdate("No download URL found.");
                return;
            }

            {
                std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
                AppState::state.updateStatusText = "DOWNLOADING UPDATE...";
            }

            std::string dlErr;
            Logger::log("[UpdateManager] Streaming archive to: " + zipFile.string());
            if (!downloadArchiveToFile(downloadUrl, zipFile, dlErr))
            {
                failUpdate("Archive download failed: " + dlErr);
                return;
            }
        }

        {
            std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
            AppState::state.updateStatusText = "EXTRACTING UPDATE...";
        }

        std::string extErr;
        Logger::log("[UpdateManager] Extracting archive via miniz into: " + extractDir.string());
        if (!extractZipArchive(zipFile, extractDir, extErr))
        {
            failUpdate("Extraction failed: " + extErr);
            return;
        }

        // Find root folder containing the application binary
#ifdef _WIN32
        const std::string binTarget = "HPR.exe";
#else
        const std::string binTarget = "HPR";
#endif
        std::filesystem::path sourceDir = extractDir;
        if (!std::filesystem::exists(sourceDir / binTarget))
        {
            for (const auto &entry : std::filesystem::recursive_directory_iterator(extractDir))
            {
                if (entry.is_regular_file() && entry.path().filename().string() == binTarget)
                {
                    sourceDir = entry.path().parent_path();
                    break;
                }
            }
        }

        std::filesystem::path appDir = getAppDirectory();
        Logger::log("[UpdateManager] App installation folder: " + appDir.string());
        Logger::log("[UpdateManager] Extracted binary folder: " + sourceDir.string());

        // Strictly check next to the application binary (no pwd fallback)
#ifdef _WIN32
        std::filesystem::path updaterExe = appDir / "HPRUpdater.exe";
#else
        std::filesystem::path updaterExe = appDir / "HPRUpdater";
#endif

        if (!std::filesystem::exists(updaterExe))
        {
            failUpdate("Updater executable not found next to binary at: " + updaterExe.string());
            return;
        }

        uint32_t currentPid = 0;
#ifdef _WIN32
        currentPid = GetCurrentProcessId();
#else
        currentPid = static_cast<uint32_t>(getpid());
#endif

        std::string args = "--pid " + std::to_string(currentPid) +
                           " --target-dir \"" + appDir.string() + "\"" +
                           " --source-dir \"" + sourceDir.string() + "\"";

        {
            std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
            AppState::state.updateStatusText = "LAUNCHING UPDATER...";
        }

        Logger::log("[UpdateManager] Spawning updater: " + updaterExe.string() + " " + args);

#ifdef _WIN32
        std::wstring wExe = updaterExe.wstring();
        std::wstring wArgs(args.begin(), args.end());
        std::wstring wDir = appDir.wstring();

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas"; // Requests UAC admin elevation
        sei.lpFile = wExe.c_str();
        sei.lpParameters = wArgs.c_str();
        sei.lpDirectory = wDir.c_str();
        sei.nShow = SW_SHOW;   // Show console window
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;

        if (!ShellExecuteExW(&sei))
        {
            failUpdate("Failed to launch updater.exe with elevation.");
            return;
        }
#else
        std::string shellCmd;
        if (access(appDir.c_str(), W_OK) != 0)
        {
            if (system("which pkexec >/dev/null 2>&1") == 0)
            {
                shellCmd = "pkexec \"" + updaterExe.string() + "\" " + args + " &";
            }
            else
            {
                shellCmd = "x-terminal-emulator -e sudo \"" + updaterExe.string() + "\" " + args + " &";
            }
        }
        else
        {
            shellCmd = "x-terminal-emulator -e \"" + updaterExe.string() + "\" " + args + " &";
        }
        system(shellCmd.c_str());
#endif

        Logger::log("[UpdateManager] Updater spawned. Shutting down SingleInstance and exiting HPR.");
        SingleInstance::getInstance().shutdown();

        slint::invoke_from_event_loop([]() {
            slint::quit_event_loop();
        });

        // Give the main thread time to exit gracefully through main()
        // If still alive after 2 seconds, terminate at the OS level without running thread-unsafe destructors
        std::this_thread::sleep_for(std::chrono::seconds(2));
#ifdef _WIN32
        ExitProcess(0);
#else
        _Exit(0);
#endif

    }).detach();
}

bool UpdateManager::extractZipArchive(const std::filesystem::path &zipPath, const std::filesystem::path &destDir, std::string &err)
{
    mz_zip_archive zipArchive;
    memset(&zipArchive, 0, sizeof(zipArchive));

    if (!mz_zip_reader_init_file(&zipArchive, zipPath.string().c_str(), 0))
    {
        err = "Failed to initialize zip reader: " + std::string(mz_zip_get_error_string(mz_zip_get_last_error(&zipArchive)));
        return false;
    }

    std::error_code ec;
    // Cleanly wipe any pre-existing extracted files in temp directory to guarantee fresh overwrite
    std::filesystem::remove_all(destDir, ec);
    std::filesystem::create_directories(destDir, ec);
    mz_uint numFiles = mz_zip_reader_get_num_files(&zipArchive);

    for (mz_uint i = 0; i < numFiles; i++)
    {
        if (numFiles > 0 && i % 3 == 0)
        {
            int pct = static_cast<int>((static_cast<double>(i) / numFiles) * 100.0);
            std::lock_guard<std::recursive_mutex> stateLock(AppState::stateMutex);
            AppState::state.updateStatusText = "EXTRACTING (" + std::to_string(pct) + "%)...";
        }

        mz_zip_archive_file_stat fileStat;
        if (!mz_zip_reader_file_stat(&zipArchive, i, &fileStat))
        {
            continue;
        }

        std::filesystem::path outPath = destDir / fileStat.m_filename;

        if (mz_zip_reader_is_file_a_directory(&zipArchive, i))
        {
            std::filesystem::create_directories(outPath, ec);
        }
        else
        {
            std::filesystem::create_directories(outPath.parent_path(), ec);
            if (std::filesystem::exists(outPath, ec))
            {
                std::filesystem::remove(outPath, ec);
            }

            if (!mz_zip_reader_extract_to_file(&zipArchive, i, outPath.string().c_str(), 0))
            {
                err = "Failed to extract " + std::string(fileStat.m_filename);
                mz_zip_reader_end(&zipArchive);
                return false;
            }
        }
    }

    mz_zip_reader_end(&zipArchive);
    return true;
}

bool UpdateManager::downloadArchiveToFile(const std::string &url, const std::filesystem::path &destPath, std::string &err)
{
#ifdef _WIN32
    std::wstring wUrl(url.begin(), url.end());
    URL_COMPONENTS urlComp = { sizeof(urlComp) };
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp))
    {
        err = "Failed to parse download URL";
        return false;
    }

    std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength);

    HINTERNET hSession = WinHttpOpen(L"HPR-Updater/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
    {
        err = "WinHttpOpen failed";
        return false;
    }

    INTERNET_PORT port = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        err = "WinHttpConnect failed";
        return false;
    }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        err = "WinHttpOpenRequest failed";
        return false;
    }

    // Enable auto-redirects
    DWORD redirectOption = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectOption, sizeof(redirectOption));

    std::wstring reqHeaders = L"User-Agent: HPR-Updater\r\nAccept: application/octet-stream\r\n";
    WinHttpAddRequestHeaders(hRequest, reqHeaders.c_str(), -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        err = "WinHttpSendRequest / ReceiveResponse failed";
        return false;
    }

    std::ofstream outFile(destPath, std::ios::binary);
    if (!outFile.is_open())
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        err = "Failed to open destination file for writing";
        return false;
    }

    DWORD dwContentLength = 0;
    DWORD dwLenSize = sizeof(dwContentLength);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &dwContentLength, &dwLenSize, WINHTTP_NO_HEADER_INDEX);

    DWORD totalDownloaded = 0;
    DWORD dwSize = 0;
    std::vector<char> buffer(64 * 1024);
    while (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0)
    {
        DWORD toRead = std::min(dwSize, static_cast<DWORD>(buffer.size()));
        DWORD dwDownloaded = 0;
        if (WinHttpReadData(hRequest, buffer.data(), toRead, &dwDownloaded) && dwDownloaded > 0)
        {
            outFile.write(buffer.data(), dwDownloaded);
            totalDownloaded += dwDownloaded;
            if (dwContentLength > 0)
            {
                int pct = static_cast<int>((static_cast<double>(totalDownloaded) / dwContentLength) * 100.0);
                std::lock_guard<std::recursive_mutex> stateLock(AppState::stateMutex);
                AppState::state.updateStatusText = "DOWNLOADING (" + std::to_string(pct) + "%)...";
            }
        }
        else
        {
            break;
        }
    }

    outFile.close();
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
#else
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        err = "Failed to initialize libcurl";
        return false;
    }

    FILE *fp = fopen(destPath.string().c_str(), "wb");
    if (!fp)
    {
        curl_easy_cleanup(curl);
        err = "Failed to open output file for writing";
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "HPR-Updater");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        err = curl_easy_strerror(res);
        return false;
    }
    return true;
#endif
}

std::filesystem::path UpdateManager::getAppDirectory()
{
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(NULL, path, MAX_PATH))
    {
        return std::filesystem::path(path).parent_path();
    }
    return std::filesystem::current_path();
#else
    try
    {
        return std::filesystem::canonical("/proc/self/exe").parent_path();
    }
    catch (...)
    {
        return std::filesystem::current_path();
    }
#endif
}
