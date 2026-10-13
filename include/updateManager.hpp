#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

class UpdateManager
{
  public:
    UpdateManager();
    ~UpdateManager();

    void run();
    void stop();
    void promptUserForUpdate();
    void startUpdatePipeline();

    static bool extractZipArchive(const std::filesystem::path &zipPath, const std::filesystem::path &destDir, std::string &err);
    static bool downloadArchiveToFile(const std::string &url, const std::filesystem::path &destPath, std::string &err);
    static std::filesystem::path getAppDirectory();

  private:
    void workerLoop();
    void checkForUpdates();

  private:
    std::atomic<bool> running{false};
    std::atomic<bool> isUpdating{false};
    std::thread workerThread;
    std::mutex updateMutex;
};
