// singleInstance.hpp
#pragma once
#include <functional>
#include <string>
#include <thread>
#include <atomic>

class SingleInstance {
public:
    static SingleInstance& getInstance();

    // Returns true if this is the first instance.
    // Returns false if another instance is already running (it notifies the active instance and exits).
    bool checkAndNotify();

    // Registers the callback to show the application window.
    void onShow(std::function<void()> callback);

    // Stops the server thread and cleans up resources.
    void shutdown();

    ~SingleInstance();

private:
    SingleInstance();
    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    void runServer();

    std::function<void()> showCallback;
    std::thread serverThread;
    std::atomic<bool> running{true};

#ifdef _WIN32
    void* hPipeServer;
    std::wstring pipeName;
#else
    int serverFd;
    std::string socketPath;
#endif
};
