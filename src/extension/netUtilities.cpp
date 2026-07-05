#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
#endif
#include "logger.hpp"
#include "netUtilities.hpp"
#include "extensionManager.hpp"
#include "appState.hpp"
#include <vector>
#include <iostream>
#include <map>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>

#ifdef _WIN32
    #include <windows.h>
    #include <winhttp.h>
    #pragma comment(lib, "winhttp.lib")
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <curl/curl.h>
#endif

namespace NativeNet
{
#ifndef _WIN32
    // Helper to write curl data
    size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }
#endif

    std::pair<std::string, int> httpGet(const std::string& host, const std::string& path, bool secure, const std::map<std::string, std::string>& headers)
    {
        if (!AppState::configManager.getConfig<bool>("allow-network-activity", true))
        {
            return { "", 0 };
        }
        std::string response;
        int statusCode = 0;

#ifdef _WIN32
        HINTERNET hSession = WinHttpOpen(L"HPR/1.0", 
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
                                         WINHTTP_NO_PROXY_NAME, 
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return { "", 0 };

        std::string hostname = host;
        INTERNET_PORT port = secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

        size_t colonPos = host.rfind(':');
        if (colonPos != std::string::npos) {
            hostname = host.substr(0, colonPos);
            port = static_cast<INTERNET_PORT>(std::stoi(host.substr(colonPos + 1)));
        }

        std::wstring wHost(hostname.begin(), hostname.end());
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return { "", 0 };
        }

        std::wstring wPath(path.begin(), path.end());
        DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
        
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(),
                                                NULL, WINHTTP_NO_REFERER, 
                                                WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return { "", 0 };
        }

        // Add custom headers
        for (const auto& [k, v] : headers) {
            std::string headerLine = k + ": " + v + "\r\n";
            std::wstring wHeaderLine(headerLine.begin(), headerLine.end());
            WinHttpAddRequestHeaders(hRequest, wHeaderLine.c_str(), -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }

        if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hRequest, NULL)) 
        {
            // Query HTTP Status Code
            DWORD dwStatusCode = 0;
            DWORD dwSize = sizeof(dwStatusCode);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_statusCode | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX)) 
            {
                statusCode = static_cast<int>(dwStatusCode);
            }

            DWORD dwSizeData = 0;
            do {
                if (WinHttpQueryDataAvailable(hRequest, &dwSizeData) && dwSizeData > 0) {
                    std::vector<char> buffer(dwSizeData + 1);
                    DWORD dwDownloaded = 0;
                    if (WinHttpReadData(hRequest, &buffer[0], dwSizeData, &dwDownloaded)) {
                        response.append(&buffer[0], dwDownloaded);
                    }
                }
            } while (dwSizeData > 0);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

#else
        CURL* curl = curl_easy_init();
        if (curl) {
            std::string url = (secure ? "https://" : "http://") + host + path;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "HPR/1.0");
            
            struct curl_slist* curlHeaders = nullptr;
            for (const auto& [k, v] : headers) {
                std::string headerStr = k + ": " + v;
                curlHeaders = curl_slist_append(curlHeaders, headerStr.c_str());
            }
            if (curlHeaders) {
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlHeaders);
            }

            CURLcode res = curl_easy_perform(curl);
            if (res == CURLE_OK) {
                long responseCode = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
                statusCode = static_cast<int>(responseCode);
            } else {
                std::cerr << "[HPR] curl failed: " << curl_easy_strerror(res) << "\n";
                Logger::log("[HPR] curl failed: " + std::string(curl_easy_strerror(res)));
            }
            if (curlHeaders) {
                curl_slist_free_all(curlHeaders);
            }
            curl_easy_cleanup(curl);
        }
#endif
        return { response, statusCode };
    }

    std::pair<std::string, int> httpPost(const std::string& host, const std::string& path, const std::string& body, bool secure, const std::map<std::string, std::string>& headers)
    {
        if (!AppState::configManager.getConfig<bool>("allow-network-activity", true))
        {
            return { "", 0 };
        }
        std::string response;
        int statusCode = 0;

#ifdef _WIN32
        HINTERNET hSession = WinHttpOpen(L"HPR/1.0", 
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
                                         WINHTTP_NO_PROXY_NAME, 
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return { "", 0 };

        std::string hostname = host;
        INTERNET_PORT port = secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

        size_t colonPos = host.rfind(':');
        if (colonPos != std::string::npos) {
            hostname = host.substr(0, colonPos);
            port = static_cast<INTERNET_PORT>(std::stoi(host.substr(colonPos + 1)));
        }

        std::wstring wHost(hostname.begin(), hostname.end());
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return { "", 0 };
        }

        std::wstring wPath(path.begin(), path.end());
        DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
        
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wPath.c_str(),
                                                NULL, WINHTTP_NO_REFERER, 
                                                WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return { "", 0 };
        }

        // Add custom headers
        bool hasContentType = false;
        for (const auto& [k, v] : headers) {
            std::string lowerK = k;
            std::transform(lowerK.begin(), lowerK.end(), lowerK.begin(), [](unsigned char c) { return std::tolower(c); });
            if (lowerK == "content-type") hasContentType = true;

            std::string headerLine = k + ": " + v + "\r\n";
            std::wstring wHeaderLine(headerLine.begin(), headerLine.end());
            WinHttpAddRequestHeaders(hRequest, wHeaderLine.c_str(), -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }
        if (!hasContentType) {
            WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json\r\n", -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }

        if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 
                               (LPVOID)body.c_str(), body.size(), body.size(), 0) &&
            WinHttpReceiveResponse(hRequest, NULL)) 
        {
            // Query HTTP Status Code
            DWORD dwStatusCode = 0;
            DWORD dwSize = sizeof(dwStatusCode);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_statusCode | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX)) 
            {
                statusCode = static_cast<int>(dwStatusCode);
            }

            DWORD dwSizeData = 0;
            do {
                if (WinHttpQueryDataAvailable(hRequest, &dwSizeData) && dwSizeData > 0) {
                    std::vector<char> buffer(dwSizeData + 1);
                    DWORD dwDownloaded = 0;
                    if (WinHttpReadData(hRequest, &buffer[0], dwSizeData, &dwDownloaded)) {
                        response.append(&buffer[0], dwDownloaded);
                    }
                }
            } while (dwSizeData > 0);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

#else
        CURL* curl = curl_easy_init();
        if (curl) {
            std::string url = (secure ? "https://" : "http://") + host + path;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "HPR/1.0");

            struct curl_slist* curlHeaders = nullptr;
            bool hasContentType = false;
            for (const auto& [k, v] : headers) {
                std::string lowerK = k;
                std::transform(lowerK.begin(), lowerK.end(), lowerK.begin(), [](unsigned char c) { return std::tolower(c); });
                if (lowerK == "content-type") hasContentType = true;

                std::string headerStr = k + ": " + v;
                curlHeaders = curl_slist_append(curlHeaders, headerStr.c_str());
            }
            if (!hasContentType) {
                curlHeaders = curl_slist_append(curlHeaders, "Content-Type: application/json");
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlHeaders);
            
            CURLcode res = curl_easy_perform(curl);
            if (res == CURLE_OK) {
                long responseCode = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
                statusCode = static_cast<int>(responseCode);
            } else {
                std::cerr << "[HPR] curl failed: " << curl_easy_strerror(res) << "\n";
                Logger::log("[HPR] curl failed: " + std::string(curl_easy_strerror(res)));
            }
            curl_slist_free_all(curlHeaders);
            curl_easy_cleanup(curl);
        }
#endif
        return { response, statusCode };
    }

    std::pair<std::string, int> httpPut(const std::string& host, const std::string& path, const std::string& body, bool secure, const std::map<std::string, std::string>& headers)
    {
        if (!AppState::configManager.getConfig<bool>("allow-network-activity", true))
        {
            return { "", 0 };
        }
        std::string response;
        int statusCode = 0;

#ifdef _WIN32
        HINTERNET hSession = WinHttpOpen(L"HPR/1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return { "", 0 };

        std::string hostname = host;
        INTERNET_PORT port = secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

        size_t colonPos = host.rfind(':');
        if (colonPos != std::string::npos) {
            hostname = host.substr(0, colonPos);
            port = static_cast<INTERNET_PORT>(std::stoi(host.substr(colonPos + 1)));
        }

        std::wstring wHost(hostname.begin(), hostname.end());
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return { "", 0 };
        }

        std::wstring wPath(path.begin(), path.end());
        DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"PUT", wPath.c_str(),
                                                NULL, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return { "", 0 };
        }

        bool hasContentType = false;
        for (const auto& [k, v] : headers) {
            std::string lowerK = k;
            std::transform(lowerK.begin(), lowerK.end(), lowerK.begin(), [](unsigned char c) { return std::tolower(c); });
            if (lowerK == "content-type") hasContentType = true;

            std::string headerLine = k + ": " + v + "\r\n";
            std::wstring wHeaderLine(headerLine.begin(), headerLine.end());
            WinHttpAddRequestHeaders(hRequest, wHeaderLine.c_str(), -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }
        if (!hasContentType) {
            WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json\r\n", -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }

        if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               (LPVOID)body.c_str(), body.size(), body.size(), 0) &&
            WinHttpReceiveResponse(hRequest, NULL))
        {
            DWORD dwStatusCode = 0;
            DWORD dwSize = sizeof(dwStatusCode);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_statusCode | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX))
            {
                statusCode = static_cast<int>(dwStatusCode);
            }

            DWORD dwSizeData = 0;
            do {
                if (WinHttpQueryDataAvailable(hRequest, &dwSizeData) && dwSizeData > 0) {
                    std::vector<char> buffer(dwSizeData + 1);
                    DWORD dwDownloaded = 0;
                    if (WinHttpReadData(hRequest, &buffer[0], dwSizeData, &dwDownloaded)) {
                        response.append(&buffer[0], dwDownloaded);
                    }
                }
            } while (dwSizeData > 0);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

#else
        CURL* curl = curl_easy_init();
        if (curl) {
            std::string url = (secure ? "https://" : "http://") + host + path;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "HPR/1.0");

            struct curl_slist* curlHeaders = nullptr;
            bool hasContentType = false;
            for (const auto& [k, v] : headers) {
                std::string lowerK = k;
                std::transform(lowerK.begin(), lowerK.end(), lowerK.begin(), [](unsigned char c) { return std::tolower(c); });
                if (lowerK == "content-type") hasContentType = true;
                std::string headerStr = k + ": " + v;
                curlHeaders = curl_slist_append(curlHeaders, headerStr.c_str());
            }
            if (!hasContentType) {
                curlHeaders = curl_slist_append(curlHeaders, "Content-Type: application/json");
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlHeaders);

            CURLcode res = curl_easy_perform(curl);
            if (res == CURLE_OK) {
                long responseCode = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
                statusCode = static_cast<int>(responseCode);
            } else {
                Logger::log("[HPR] curl PUT failed: " + std::string(curl_easy_strerror(res)));
            }
            curl_slist_free_all(curlHeaders);
            curl_easy_cleanup(curl);
        }
#endif
        return { response, statusCode };
    }

    namespace
    {
#ifdef _WIN32
        typedef int socklen_t;
        #define CLOSE_SOCKET closesocket
        #define INVALID_SOCKET_VAL INVALID_SOCKET
        #define SOCKET_ERROR_VAL SOCKET_ERROR
#else
        #define CLOSE_SOCKET ::close
        #define INVALID_SOCKET_VAL -1
        #define SOCKET_ERROR_VAL -1
        typedef int SOCKET;
#endif

        // Read client request, parse HTTP, invoke Lua handler, and respond
        void handleClient(SOCKET clientSocket, sol::function handler, sol::state& lua)
        {
            std::string request_str;
            char buffer[1024];
            int bytes_received;

            // 1. Read headers
            size_t header_end = std::string::npos;
            while (true)
            {
                bytes_received = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
                if (bytes_received <= 0)
                {
                    break;
                }
                buffer[bytes_received] = '\0';
                request_str.append(buffer, bytes_received);

                header_end = request_str.find("\r\n\r\n");
                if (header_end != std::string::npos)
                {
                    break;
                }
            }

            if (header_end == std::string::npos)
            {
                CLOSE_SOCKET(clientSocket);
                return;
            }

            std::string headers_part = request_str.substr(0, header_end);
            std::string body_part = request_str.substr(header_end + 4);

            // 2. Parse request line (e.g. "POST /api/v1/buckets HTTP/1.1")
            std::string method = "GET";
            std::string path = "/";
            size_t first_line_end = headers_part.find("\r\n");
            std::string request_line = (first_line_end != std::string::npos) ? headers_part.substr(0, first_line_end) : headers_part;

            std::vector<std::string> req_tokens;
            size_t start = 0;
            for (size_t i = 0; i <= request_line.length(); ++i)
            {
                if (i == request_line.length() || request_line[i] == ' ')
                {
                    if (i > start)
                    {
                        req_tokens.push_back(request_line.substr(start, i - start));
                    }
                    start = i + 1;
                }
            }

            if (req_tokens.size() >= 2)
            {
                method = req_tokens[0];
                path = req_tokens[1];
            }

            // 3. Parse headers
            std::map<std::string, std::string> headers;
            size_t pos = (first_line_end != std::string::npos) ? first_line_end + 2 : std::string::npos;
            size_t content_length = 0;

            while (pos != std::string::npos && pos < headers_part.length())
            {
                size_t line_end = headers_part.find("\r\n", pos);
                std::string line = (line_end != std::string::npos) ? headers_part.substr(pos, line_end - pos) : headers_part.substr(pos);

                size_t colon = line.find(':');
                if (colon != std::string::npos)
                {
                    std::string key = line.substr(0, colon);
                    std::string value = line.substr(colon + 1);

                    // Trim key and value
                    while (!key.empty() && std::isspace(static_cast<unsigned char>(key.front()))) key.erase(key.begin());
                    while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
                    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
                    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();

                    std::string lower_key = key;
                    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), [](unsigned char c) { return std::tolower(c); });

                    headers[lower_key] = value;

                    if (lower_key == "content-length")
                    {
                        try
                        {
                            content_length = std::stoull(value);
                        }
                        catch (...)
                        {
                            content_length = 0;
                        }
                    }
                }

                if (line_end == std::string::npos) break;
                pos = line_end + 2;
            }

            // 4. Read body if incomplete
            while (body_part.length() < content_length)
            {
                size_t to_read = content_length - body_part.length();
                size_t chunk_size = (to_read > sizeof(buffer) - 1) ? sizeof(buffer) - 1 : to_read;
                bytes_received = recv(clientSocket, buffer, static_cast<int>(chunk_size), 0);
                if (bytes_received <= 0)
                {
                    break;
                }
                buffer[bytes_received] = '\0';
                body_part.append(buffer, bytes_received);
            }

            // 5. Invoke Lua handler
            sol::table req_table = lua.create_table();
            req_table["method"] = method;
            req_table["path"] = path;
            req_table["body"] = body_part;

            sol::table headers_table = lua.create_table();
            for (const auto& [k, v] : headers)
            {
                headers_table[k] = v;
            }
            req_table["headers"] = headers_table;

            int statusCode = 200;
            std::string responseBody;
            std::map<std::string, std::string> responseHeaders;

            sol::protected_function_result result = handler(req_table);
            if (result.valid())
            {
                sol::object res_obj = result;
                if (res_obj.is<sol::table>())
                {
                    sol::table res_table = res_obj.as<sol::table>();
                    sol::optional<int> status_opt = res_table["status"];
                    if (status_opt) statusCode = *status_opt;

                    sol::optional<std::string> body_opt = res_table["body"];
                    if (body_opt) responseBody = *body_opt;

                    sol::optional<sol::table> headers_opt = res_table["headers"];
                    if (headers_opt)
                    {
                        headers_opt->for_each([&](sol::object k, sol::object v) {
                            if (k.is<std::string>() && v.is<std::string>())
                            {
                                responseHeaders[k.as<std::string>()] = v.as<std::string>();
                            }
                        });
                    }
                }
                else if (res_obj.is<std::string>())
                {
                    responseBody = res_obj.as<std::string>();
                }
            }
            else
            {
                sol::error err = result;
                std::cerr << "[HPR HTTP Server] Lua Handler Error: " << err.what() << std::endl;
                Logger::log("[HPR HTTP Server] Lua Handler Error: " + std::string(err.what()));
                statusCode = 500;
                responseBody = "Internal Server Error: " + std::string(err.what());
            }

            // 6. Format and Send response
            std::string response = "HTTP/1.1 " + std::to_string(statusCode) + " OK\r\n";
            response += "Connection: close\r\n";
            bool hasContentLength = false;
            for (const auto& [k, v] : responseHeaders)
            {
                std::string lower_k = k;
                std::transform(lower_k.begin(), lower_k.end(), lower_k.begin(), [](unsigned char c) { return std::tolower(c); });
                if (lower_k == "content-length") hasContentLength = true;
                response += k + ": " + v + "\r\n";
            }
            if (!hasContentLength)
            {
                response += "Content-Length: " + std::to_string(responseBody.length()) + "\r\n";
            }
            response += "\r\n";
            response += responseBody;

            send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
            CLOSE_SOCKET(clientSocket);
        }
    }

    bool startHttpServer(int port, sol::function handler, LoadedExtension& ext)
    {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            std::cerr << "[HPR HTTP Server] WSAStartup failed" << std::endl;
            Logger::log("[HPR HTTP Server] WSAStartup failed");
            return false;
        }
#endif

        SOCKET serverFd = socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd == INVALID_SOCKET_VAL)
        {
            std::cerr << "[HPR HTTP Server] Failed to create socket" << std::endl;
            Logger::log("[HPR HTTP Server] Failed to create socket");
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }

        int opt = 1;
#ifdef _WIN32
        setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // Localhost only for safety & simplicity!
        address.sin_port = htons(port);

        if (bind(serverFd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR_VAL)
        {
            std::cerr << "[HPR HTTP Server] Failed to bind to port " << port << std::endl;
            Logger::log("[HPR HTTP Server] Failed to bind to port " + std::to_string(port));
            CLOSE_SOCKET(serverFd);
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }

        if (listen(serverFd, 10) == SOCKET_ERROR_VAL)
        {
            std::cerr << "[HPR HTTP Server] Listen failed" << std::endl;
            Logger::log("[HPR HTTP Server] Listen failed");
            CLOSE_SOCKET(serverFd);
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }

        
        std::cout << "[HPR HTTP Server] Server running on 127.0.0.1:" << port << std::endl;
        Logger::log("[HPR HTTP Server] Server running on 127.0.0.1:" + std::to_string(port));

        std::thread([serverFd, handler, &ext]() mutable
        {
            while (ext.running)
            {
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(serverFd, &readfds);

                timeval timeout;
                timeout.tv_sec = 0;
                timeout.tv_usec = 100000;

                int activity = select(static_cast<int>(serverFd + 1), &readfds, nullptr, nullptr, &timeout);

                if (activity < 0)
                {
                    #ifdef _WIN32
                        int err = WSAGetLastError();
                        if (err == WSAEINTR) continue;
                    #else
                        if (errno == EINTR) continue;
                    #endif
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                if (activity == 0) continue;

                if (FD_ISSET(serverFd, &readfds))
                {
                    sockaddr_in clientAddr;
                    socklen_t addrLen = sizeof(clientAddr);
                    SOCKET clientSocket = accept(serverFd, (struct sockaddr*)&clientAddr, &addrLen);
                    if (clientSocket != INVALID_SOCKET_VAL)
                    {
                        std::lock_guard<std::recursive_mutex> lock(ext.luaMutex);
                        handleClient(clientSocket, handler, ext.lua);
                    }
                }
            }

            CLOSE_SOCKET(serverFd);
            #ifdef _WIN32
                WSACleanup();
            #endif
            std::cout << "[HPR HTTP Server] Server stopped cleanly" << std::endl;
            Logger::log("[HPR HTTP Server] Server stopped cleanly");
        }).detach();

        return true;
    }
}
