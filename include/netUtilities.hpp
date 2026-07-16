#ifndef NET_UTILITIES_HPP
#define NET_UTILITIES_HPP

#include "sol.hpp"
#include <string>
#include <utility>

#include <map>

struct LoadedExtension;

namespace NativeNet
{
std::pair<std::string, int> httpGet(const std::string &host, const std::string &path, bool secure = true,
									const std::map<std::string, std::string> &headers = {});
std::pair<std::string, int> httpPost(const std::string &host, const std::string &path, const std::string &body,
									 bool secure = true, const std::map<std::string, std::string> &headers = {});
std::pair<std::string, int> httpPut(const std::string &host, const std::string &path, const std::string &body,
									bool secure = true, const std::map<std::string, std::string> &headers = {});
std::pair<std::string, int> httpDelete(const std::string &host, const std::string &path, bool secure = true,
									   const std::map<std::string, std::string> &headers = {});
bool startHttpServer(int port, sol::function handler, LoadedExtension &ext);
} // namespace NativeNet

#endif // NET_UTILITIES_HPP
