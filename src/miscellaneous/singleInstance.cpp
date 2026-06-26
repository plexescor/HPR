// singleInstance.cpp
#include "singleInstance.hpp"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#endif

SingleInstance& SingleInstance::getInstance() {
    static SingleInstance instance;
    return instance;
}

SingleInstance::SingleInstance() {
#ifdef _WIN32
    hPipeServer = nullptr;
    wchar_t username[256];
    DWORD len = GetEnvironmentVariableW(L"USERNAME", username, 256);
    pipeName = L"\\\\.\\pipe\\HPR_SingleInstance_Pipe_";
    if (len > 0) {
        pipeName += username;
    } else {
        pipeName += L"default";
    }
#else
    serverFd = -1;
    const char* user = getenv("USER");
    socketPath = "/tmp/hpr_single_instance_";
    if (user) {
        socketPath += user;
    } else {
        socketPath += "default";
    }
    socketPath += ".sock";
#endif
}

SingleInstance::~SingleInstance() {
    shutdown();
}

bool SingleInstance::checkAndNotify() {
#ifdef _WIN32
    HANDLE hPipe = CreateFileW(
        pipeName.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    if (hPipe != INVALID_HANDLE_VALUE) {
        // Active instance exists! Notify it.
        DWORD bytesWritten;
        WriteFile(hPipe, "show", 4, &bytesWritten, NULL);
        CloseHandle(hPipe);
        return false; // Not the first instance
    }
    
    // First instance. Start server.
    running = true;
    serverThread = std::thread(&SingleInstance::runServer, this);
    return true;
#else
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock != -1) {
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            // Active instance exists! Notify it.
            write(sock, "show", 4);
            close(sock);
            return false; // Not the first instance
        }
        close(sock);
    }

    // Clean up stale socket file if any
    unlink(socketPath.c_str());

    serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverFd == -1) {
        return true; // fallback
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(serverFd);
        serverFd = -1;
        return true; // fallback
    }

    if (listen(serverFd, 5) == -1) {
        close(serverFd);
        serverFd = -1;
        return true; // fallback
    }

    running = true;
    serverThread = std::thread(&SingleInstance::runServer, this);
    return true;
#endif
}

void SingleInstance::onShow(std::function<void()> callback) {
    showCallback = callback;
}

void SingleInstance::shutdown() {
    if (!running) return;
    running = false;

#ifdef _WIN32
    // Connect to the pipe ourselves to unblock ConnectNamedPipe
    HANDLE hPipe = CreateFileW(
        pipeName.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    if (hPipe != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        WriteFile(hPipe, "exit", 4, &bytesWritten, NULL);
        CloseHandle(hPipe);
    }
#else
    if (serverFd != -1) {
        ::shutdown(serverFd, SHUT_RDWR);
        close(serverFd);
        serverFd = -1;
    }
#endif

    if (serverThread.joinable()) {
        serverThread.join();
    }
}

void SingleInstance::runServer() {
#ifdef _WIN32
    while (running) {
        HANDLE hPipe = CreateNamedPipeW(
            pipeName.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,
            1024,
            1024,
            0,
            NULL
        );
        if (hPipe == INVALID_HANDLE_VALUE) {
            break;
        }

        hPipeServer = hPipe;

        BOOL connected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected && running) {
            char buffer[128];
            DWORD bytesRead;
            if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
                buffer[bytesRead] = '\0';
                if (std::string(buffer) == "show" && showCallback) {
                    showCallback();
                }
            }
        }
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
#else
    while (running) {
        int client_fd = accept(serverFd, NULL, NULL);
        if (client_fd < 0) {
            break;
        }
        if (running) {
            char buffer[128];
            ssize_t bytesRead = read(client_fd, buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                if (std::string(buffer) == "show" && showCallback) {
                    showCallback();
                }
            }
        }
        close(client_fd);
    }
#endif
}
