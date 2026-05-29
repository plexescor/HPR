#ifndef NET_UTILITIES_HPP
#define NET_UTILITIES_HPP

#include <string>
#include <utility>
#include "sol.hpp"

struct LoadedExtension;

namespace NativeNet
{
    std::pair<std::string, int> httpGet(const std::string& host, const std::string& path, bool secure = true);
    std::pair<std::string, int> httpPost(const std::string& host, const std::string& path, const std::string& body, bool secure = true);
    bool startHttpServer(int port, sol::function handler, LoadedExtension& ext);
}

#endif // NET_UTILITIES_HPP
