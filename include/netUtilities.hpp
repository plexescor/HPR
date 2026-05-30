#ifndef NET_UTILITIES_HPP
#define NET_UTILITIES_HPP

#include <string>
#include <utility>
#include "sol.hpp"

#include <map>

struct LoadedExtension;

namespace NativeNet
{
    std::pair<std::string, int> httpGet(const std::string& host, const std::string& path, bool secure = true, const std::map<std::string, std::string>& headers = {});
    std::pair<std::string, int> httpPost(const std::string& host, const std::string& path, const std::string& body, bool secure = true, const std::map<std::string, std::string>& headers = {});
    bool startHttpServer(int port, sol::function handler, LoadedExtension& ext);
}

#endif // NET_UTILITIES_HPP
