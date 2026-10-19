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

    #ifdef _WIN32
    const std::string targetAsset = "UPDATE_PACKAGE_WINDOWS.zip";
    #else
        const std::string targetAsset = "UPDATE_PACKAGE_LINUX.zip";
    #endif

    std::string downloadUrl = "";
    std::string browserKey = "\"browser_download_url\":";
    std::string nameKey = "\"name\":";
    size_t searchPos = 0;

    while (true)
    {
        auto namePos = body.find(nameKey, searchPos);
        if (namePos == std::string::npos) break;

        auto nStart = body.find('"', namePos + nameKey.length());
        if (nStart == std::string::npos) break;
        auto nEnd = body.find('"', nStart + 1);
        if (nEnd == std::string::npos) break;

        std::string assetName = body.substr(nStart + 1, nEnd - nStart - 1);
        searchPos = nEnd + 1;

        if (assetName == targetAsset)
        {
            auto dlPos = body.find(browserKey, searchPos);
            if (dlPos != std::string::npos)
            {
                auto dStart = body.find('"', dlPos + browserKey.length());
                if (dStart != std::string::npos)
                {
                    auto dEnd = body.find('"', dStart + 1);
                    if (dEnd != std::string::npos)
                    {
                        downloadUrl = body.substr(dStart + 1, dEnd - dStart - 1);
                    }
                }
            }
            break;
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
        std::string updaterCmd = "\"" + updaterExe.string() + "\" " + args;

        std::string elevatedCmd = updaterCmd;
        if (access(appDir.c_str(), W_OK) != 0)
        {
            if (system("which pkexec >/dev/null 2>&1") == 0)
                elevatedCmd = "pkexec " + updaterCmd;
            else
                elevatedCmd = "sudo " + updaterCmd;
        }

        // Terminal fallback chain
        if      (system("which konsole >/dev/null 2>&1") == 0)
            shellCmd = "konsole -e " + elevatedCmd + " &";
        else if (system("which gnome-terminal >/dev/null 2>&1") == 0)
            shellCmd = "gnome-terminal -- " + elevatedCmd + " &";
        else if (system("which ptyxis >/dev/null 2>&1") == 0)
            shellCmd = "ptyxis -- " + elevatedCmd + " &";
        else if (system("which xfce4-terminal >/dev/null 2>&1") == 0)
            shellCmd = "xfce4-terminal -e " + elevatedCmd + " &";
        else if (system("which tilix >/dev/null 2>&1") == 0)
            shellCmd = "tilix -e " + elevatedCmd + " &";
        else if (system("which terminator >/dev/null 2>&1") == 0)
            shellCmd = "terminator -e " + elevatedCmd + " &";
        else if (system("which wezterm >/dev/null 2>&1") == 0)
            shellCmd = "wezterm -e " + elevatedCmd + " &";
        else if (system("which alacritty >/dev/null 2>&1") == 0)
            shellCmd = "alacritty -e " + elevatedCmd + " &";
        else if (system("which kitty >/dev/null 2>&1") == 0)
            shellCmd = "kitty " + elevatedCmd + " &";
        else if (system("which hyper >/dev/null 2>&1") == 0)
            shellCmd = "hyper " + elevatedCmd + " &";
        else if (system("which guake >/dev/null 2>&1") == 0)
            shellCmd = "guake -e " + elevatedCmd + " &";
        else if (system("which tilda >/dev/null 2>&1") == 0)
            shellCmd = "tilda -c " + elevatedCmd + " &";
        else if (system("which foot >/dev/null 2>&1") == 0)
            shellCmd = "foot " + elevatedCmd + " &";
        else if (system("which rio >/dev/null 2>&1") == 0)
            shellCmd = "rio -e " + elevatedCmd + " &";
        else if (system("which contour >/dev/null 2>&1") == 0)
            shellCmd = "contour " + elevatedCmd + " &";
        else if (system("which cool-retro-term >/dev/null 2>&1") == 0)
            shellCmd = "cool-retro-term -e " + elevatedCmd + " &";
        else if (system("which mate-terminal >/dev/null 2>&1") == 0)
            shellCmd = "mate-terminal -e " + elevatedCmd + " &";
        else if (system("which lxterminal >/dev/null 2>&1") == 0)
            shellCmd = "lxterminal -e " + elevatedCmd + " &";
        else if (system("which qterminal >/dev/null 2>&1") == 0)
            shellCmd = "qterminal -e " + elevatedCmd + " &";
        else if (system("which terminology >/dev/null 2>&1") == 0)
            shellCmd = "terminology -e " + elevatedCmd + " &";
        else if (system("which st >/dev/null 2>&1") == 0)
            shellCmd = "st -e " + elevatedCmd + " &";
        else if (system("which urxvt >/dev/null 2>&1") == 0)
            shellCmd = "urxvt -e " + elevatedCmd + " &";
        else if (system("which rxvt >/dev/null 2>&1") == 0)
            shellCmd = "rxvt -e " + elevatedCmd + " &";
        else if (system("which xterm >/dev/null 2>&1") == 0)
            shellCmd = "xterm -e " + elevatedCmd + " &";
        else
        {
            failUpdate("No terminal emulator found on this system.");
            return;
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

void UpdateManager::updateHPRStore(std::filesystem::path &path)
{
    // check if another update is already doing its thing
    if (isUpdating.exchange(true))
    {
        Logger::log("[UpdateManager] Another update is already running, skipping store update.");
        return;
    }

    // copy path so the async thread doesn't read dangling reference
    std::filesystem::path targetPath = path;

    // save if hpr update button was visible before we take over the topbar
    bool wasHprUpdateAvailable = false;
    {
        std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
        wasHprUpdateAvailable = AppState::state.updateAvailable;
        AppState::state.isUpdating = true;
        AppState::state.updateStatusText = "CHECKING STORE UPDATE...";
    }

    // kick off thread so we dont lock up the main ui thread
    std::thread([this, targetPath, wasHprUpdateAvailable]() {
        // bail out helper: logs it, updates topbar to failed, shows popup and resets state
        auto failStoreUpdate = [this, wasHprUpdateAvailable](const std::string &reason) {
            Logger::log("[UpdateManager] HPR Store update failed: " + reason);
            {
                std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
                AppState::state.updateStatusText = "UPDATE FAILED";
            }

            if (auto mgr = AppState::extManager)
            {
                std::string popupMsg = "HPR-Store update failed!\n\nReason: " + reason;
                mgr->showUiPopup(popupMsg, "CLOSE", "OK", false, [](int) {});
            }

            std::this_thread::sleep_for(std::chrono::seconds(3));
            {
                std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
                AppState::state.isUpdating = false;
                AppState::state.updateStatusText = "Update HPR";
                AppState::state.updateAvailable = wasHprUpdateAvailable;
            }
            isUpdating = false;
        };

        if (!std::filesystem::exists(targetPath))
        {
            failStoreUpdate("Path does not exist: " + targetPath.string());
            return;
        }

        Logger::log("[UpdateManager] Checking for HPR-Store update...");

        // hit the github api and get the latest release info
        std::map<std::string, std::string> headers = {
            {"User-Agent", "HPR-App"},
            {"Accept",     "application/vnd.github.v3+json"}
        };

        auto [body, status] = NativeNet::httpGet("api.github.com", "/repos/plexescor/HPR-Store/releases/latest", true, headers);
        if (status != 200 || body.empty())
        {
            failStoreUpdate("Failed to fetch release info from GitHub (HTTP " + std::to_string(status) + ")");
            return;
        }

        // grab the tag_name from json
        std::string tagKey   = "\"tag_name\":";
        auto tagPos          = body.find(tagKey);
        if (tagPos == std::string::npos)
        {
            failStoreUpdate("tag_name not found in release JSON");
            return;
        }
        auto startQuote = body.find('"', tagPos + tagKey.length());
        auto endQuote   = (startQuote != std::string::npos) ? body.find('"', startQuote + 1) : std::string::npos;
        if (startQuote == std::string::npos || endQuote == std::string::npos)
        {
            failStoreUpdate("Could not parse tag_name value from release JSON");
            return;
        }
        std::string rawTag = body.substr(startQuote + 1, endQuote - startQuote - 1);
        std::string normTag = rawTag;
        if (!normTag.empty() && (normTag[0] == 'v' || normTag[0] == 'V'))
            normTag = normTag.substr(1);

        Logger::log("[UpdateManager] HPR Store latest release tag: " + normTag);

        // now find the download url for HPR-Store.zip in the assets list
        const std::string targetAsset = "HPR-Store.zip";
        std::string downloadUrl;
        std::string browserKey = "\"browser_download_url\":";
        std::string nameKey    = "\"name\":";
        size_t searchPos = 0;

        while (true)
        {
            auto namePos = body.find(nameKey, searchPos);
            if (namePos == std::string::npos) break;

            auto nStart = body.find('"', namePos + nameKey.length());
            if (nStart == std::string::npos) break;
            auto nEnd = body.find('"', nStart + 1);
            if (nEnd == std::string::npos) break;

            std::string assetName = body.substr(nStart + 1, nEnd - nStart - 1);
            searchPos = nEnd + 1;

            if (assetName == targetAsset)
            {
                auto dlPos = body.find(browserKey, searchPos);
                if (dlPos != std::string::npos)
                {
                    auto dStart = body.find('"', dlPos + browserKey.length());
                    if (dStart != std::string::npos)
                    {
                        auto dEnd = body.find('"', dStart + 1);
                        if (dEnd != std::string::npos)
                            downloadUrl = body.substr(dStart + 1, dEnd - dStart - 1);
                    }
                }
                break;
            }
        }

        if (downloadUrl.empty())
        {
            failStoreUpdate("Asset '" + targetAsset + "' not found in GitHub release");
            return;
        }

        {
            std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
            AppState::state.updateStatusText = "DOWNLOADING STORE...";
        }

        // download the zip to temp
        std::filesystem::path tempBase = std::filesystem::temp_directory_path() / "hpr-store-update";
        std::error_code ec;
        std::filesystem::create_directories(tempBase, ec);

        std::filesystem::path zipFile    = tempBase / "HPR-Store.zip";
        std::filesystem::path extractDir = tempBase / "extracted";

        Logger::log("[UpdateManager] Downloading HPR-Store archive to: " + zipFile.string());
        std::string dlErr;
        if (!downloadArchiveToFile(downloadUrl, zipFile, dlErr))
        {
            std::filesystem::remove_all(tempBase, ec);
            failStoreUpdate("Download failed: " + dlErr);
            return;
        }

        {
            std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
            AppState::state.updateStatusText = "EXTRACTING STORE...";
        }

        // unzip it
        Logger::log("[UpdateManager] Extracting HPR-Store archive to: " + extractDir.string());
        std::string extErr;
        if (!extractZipArchive(zipFile, extractDir, extErr))
        {
            std::filesystem::remove_all(tempBase, ec);
            failStoreUpdate("Extraction failed: " + extErr);
            return;
        }

        // zip might have a top level folder so find the actual root by looking for the dll/so
#ifdef _WIN32
        const std::string sentinel = "HPR-Store.dll";
#else
        const std::string sentinel = "HPR-Store.so";
#endif

        std::filesystem::path sourceDir = extractDir;
        if (!std::filesystem::exists(sourceDir / sentinel))
        {
            for (const auto &entry : std::filesystem::recursive_directory_iterator(extractDir, ec))
            {
                if (entry.is_regular_file() && entry.path().filename().string() == sentinel)
                {
                    sourceDir = entry.path().parent_path();
                    break;
                }
            }
        }

        if (!std::filesystem::exists(sourceDir / sentinel))
        {
            std::filesystem::remove_all(tempBase, ec);
            failStoreUpdate("Sentinel file '" + sentinel + "' not found in extracted store archive");
            return;
        }

        Logger::log("[UpdateManager] HPR Store source dir resolved to: " + sourceDir.string());

        {
            std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
            AppState::state.updateStatusText = "INSTALLING STORE...";
        }

        // just yeet all the files over, overwrite whatever is there
        try
        {
            std::filesystem::copy(
                sourceDir, targetPath,
                std::filesystem::copy_options::recursive |
                std::filesystem::copy_options::overwrite_existing,
                ec);

            if (ec)
            {
                std::filesystem::remove_all(tempBase, ec);
                failStoreUpdate("File copy failed: " + ec.message());
                return;
            }
        }
        catch (const std::exception &e)
        {
            std::filesystem::remove_all(tempBase, ec);
            failStoreUpdate("Exception during file replacement: " + std::string(e.what()));
            return;
        }

        Logger::log("[UpdateManager] HPR Store files successfully updated to version " + normTag + " at: " + targetPath.string());

        {
            std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
            AppState::state.updateStatusText = "CONFIGURING STORE...";
        }

        // update installed.json so it knows the new version
        updateHPRStore_Config(targetPath, normTag);

        // clean up temp stuff
        std::filesystem::remove_all(tempBase, ec);

        Logger::log("[UpdateManager] HPR Store update completed successfully to version " + normTag);
        {
            std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
            AppState::state.updateStatusText = "STORE UPDATED";
        }

        if (auto mgr = AppState::extManager)
        {
            std::string popupMsg = "HPR-Store has been successfully updated to version " + normTag + "!\n\n"
                                   "Please restart HPR to load the updated store.";
            mgr->showUiPopup(popupMsg, "CLOSE", "OK", false, 
                [](int button) 
                {
                    std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
                    if (button == 1 && AppState::extManager)
                    {
                        AppState::extManager->refresh();
                    }
                });
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        {
            std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
            AppState::state.isUpdating = false;
            AppState::state.updateStatusText = "Update HPR";
            AppState::state.updateAvailable = wasHprUpdateAvailable;
        }
        isUpdating = false;
    }).detach();
}

void UpdateManager::updateHPRStore_Config(const std::filesystem::path &installPath, const std::string &newVersion)
{
    // figure out where %APPDATA%/HPR-Store is, same as storeBasePath() in the store
    std::filesystem::path storeBase;
#ifdef _WIN32
    const char *appdata = std::getenv("APPDATA");
    if (appdata)
        storeBase = std::filesystem::path(appdata) / "HPR-Store";
#else
    const char *home = std::getenv("HOME");
    if (home)
        storeBase = std::filesystem::path(home) / ".config" / "HPR-Store";
#endif

    if (storeBase.empty())
    {
        Logger::log("[UpdateManager] HPR Store Config: Could not resolve store base path.");
        return;
    }

    std::filesystem::path installedPath = storeBase / "installed.json";

    // read whatever is in installed.json already, or start fresh if it doesnt exist
    std::string existingContent = "{}";
    if (std::filesystem::exists(installedPath))
    {
        std::ifstream in(installedPath);
        if (in.is_open())
        {
            existingContent.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            in.close();
        }
    }
    else
    {
        // make sure the dir exists
        std::error_code ec;
        std::filesystem::create_directories(storeBase, ec);
    }

    // we dont wanna pull in a json lib so just do it manually
    // remove old hpr-store entry if there is one, then stick in the new one

    const std::string entryKey  = "\"hpr-store\"";
    std::string folderName      = installPath.filename().string();

    // build the new entry string
    std::string newEntry =
        "  \"hpr-store\": {\n"
        "    \"folder\": \"" + folderName + "\",\n"
        "    \"version\": \"" + newVersion + "\",\n"
        "    \"authorName\": \"\",\n"
        "    \"extensionName\": \"HPR Store\"\n"
        "  }";

    // remove the existing hpr-store block if its already there
    auto keyPos = existingContent.find(entryKey);
    if (keyPos != std::string::npos)
    {
        // find the opening brace of the value object
        auto braceOpen = existingContent.find('{', keyPos + entryKey.length());
        if (braceOpen != std::string::npos)
        {
            int depth = 0;
            size_t braceClose = braceOpen;
            for (size_t i = braceOpen; i < existingContent.size(); ++i)
            {
                if (existingContent[i] == '{') ++depth;
                else if (existingContent[i] == '}') { --depth; if (depth == 0) { braceClose = i; break; } }
            }
            // eat trailing comma too if there is one
            size_t removeEnd = braceClose + 1;
            if (removeEnd < existingContent.size() && existingContent[removeEnd] == ',')
                ++removeEnd;
            // eat leading comma before the key if any
            size_t removeStart = keyPos;
            if (removeStart > 0 && existingContent[removeStart - 1] == ',')
                --removeStart;
            existingContent.erase(removeStart, removeEnd - removeStart);
        }
    }

    // find the last } and shove our entry in before it
    auto outerClose = existingContent.rfind('}');
    if (outerClose == std::string::npos)
    {
        // json is busted, start over
        existingContent = "{}";
        outerClose = existingContent.rfind('}');
    }

    // need a comma if there are other entries already in there
    auto outerOpen = existingContent.find('{');
    bool hasOtherEntries = false;
    if (outerOpen != std::string::npos && outerOpen < outerClose)
    {
        for (size_t i = outerOpen + 1; i < outerClose; ++i)
        {
            if (!std::isspace(static_cast<unsigned char>(existingContent[i])))
            {
                hasOtherEntries = true;
                break;
            }
        }
    }

    std::string injection = (hasOtherEntries ? ",\n" : "\n") + newEntry + "\n";
    existingContent.insert(outerClose, injection);

    // write it back
    std::ofstream out(installedPath, std::ios::trunc);
    if (!out.is_open())
    {
        Logger::log("[UpdateManager] HPR Store Config: Failed to write installed.json at: " + installedPath.string());
        return;
    }
    out << existingContent;
    out.close();

    Logger::log("[UpdateManager] HPR Store Config: Updated installed.json (hpr-store version: " + newVersion + ") at: " + installedPath.string());
}