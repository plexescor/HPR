#include "netUtilities.hpp"
#include <vector>
#include <iostream>

#ifdef _WIN32
    #include <windows.h>
    #include <winhttp.h>
    #pragma comment(lib, "winhttp.lib")
#else
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

    std::string httpGet(const std::string& host, const std::string& path, bool secure)
    {
        std::string response;

#ifdef _WIN32
        HINTERNET hSession = WinHttpOpen(L"HPR/1.0", 
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
                                         WINHTTP_NO_PROXY_NAME, 
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return "";

        std::wstring wHost(host.begin(), host.end());
        INTERNET_PORT port = secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
        
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return "";
        }

        std::wstring wPath(path.begin(), path.end());
        DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
        
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(),
                                                NULL, WINHTTP_NO_REFERER, 
                                                WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hRequest, NULL)) 
        {
            DWORD dwSize = 0;
            do {
                if (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
                    std::vector<char> buffer(dwSize + 1);
                    DWORD dwDownloaded = 0;
                    if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) {
                        response.append(&buffer[0], dwDownloaded);
                    }
                }
            } while (dwSize > 0);
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
            
            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                std::cerr << "[HPR] curl failed: " << curl_easy_strerror(res) << "\n";
            }
            curl_easy_cleanup(curl);
        }
#endif
        return response;
    }

    std::string httpPost(const std::string& host, const std::string& path, const std::string& body, bool secure)
    {
        std::string response;

#ifdef _WIN32
        HINTERNET hSession = WinHttpOpen(L"HPR/1.0", 
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
                                         WINHTTP_NO_PROXY_NAME, 
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return "";

        std::wstring wHost(host.begin(), host.end());
        INTERNET_PORT port = secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
        
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return "";
        }

        std::wstring wPath(path.begin(), path.end());
        DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
        
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wPath.c_str(),
                                                NULL, WINHTTP_NO_REFERER, 
                                                WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        LPCWSTR additionalHeaders = L"Content-Type: application/json\r\n";
        if (WinHttpSendRequest(hRequest, additionalHeaders, -1L, 
                               (LPVOID)body.c_str(), body.size(), body.size(), 0) &&
            WinHttpReceiveResponse(hRequest, NULL)) 
        {
            DWORD dwSize = 0;
            do {
                if (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
                    std::vector<char> buffer(dwSize + 1);
                    DWORD dwDownloaded = 0;
                    if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) {
                        response.append(&buffer[0], dwDownloaded);
                    }
                }
            } while (dwSize > 0);
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

            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            
            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                std::cerr << "[HPR] curl failed: " << curl_easy_strerror(res) << "\n";
            }
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }
#endif
        return response;
    }
}
