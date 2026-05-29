#ifndef NET_UTILITIES_HPP
#define NET_UTILITIES_HPP

#include <string>

namespace NativeNet
{
    std::string httpGet(const std::string& host, const std::string& path, bool secure = true);
}

#endif // NET_UTILITIES_HPP
