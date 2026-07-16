#ifndef JSON_UTILITIES_HPP
#define JSON_UTILITIES_HPP

#include "sol.hpp"
#include <string>

namespace JsonParser
{
sol::object parseJSON_E(sol::state &lua, const std::string &jsonStr, sol::optional<std::string> fieldPath);
std::string toJSON_E(const sol::object &obj);
} // namespace JsonParser

#endif // JSON_UTILITIES_HPP
