#include "jsonUtilities.hpp"
#include <sstream>
#include <cctype>
#include <iostream>

namespace JsonParser
{
    namespace
    {
        void skipWhitespace(const std::string& str, size_t& pos) {
            while (pos < str.size() && (str[pos] == ' ' || str[pos] == '\t' || str[pos] == '\n' || str[pos] == '\r')) {
                pos++;
            }
        }

        sol::object parseValue(sol::state& lua, const std::string& str, size_t& pos);

        std::string parseString(const std::string& str, size_t& pos) {
            pos++; // Skip opening double quote
            std::string result;
            while (pos < str.size()) {
                char c = str[pos];
                if (c == '"') {
                    pos++; // Skip closing quote
                    return result;
                } else if (c == '\\' && pos + 1 < str.size()) {
                    pos++; // Skip backslash
                    char esc = str[pos];
                    if (esc == 'n') result += '\n';
                    else if (esc == 't') result += '\t';
                    else if (esc == 'r') result += '\r';
                    else if (esc == 'b') result += '\b';
                    else if (esc == 'f') result += '\f';
                    else result += esc;
                } else {
                    result += c;
                }
                pos++;
            }
            return result;
        }

        sol::object parseObject(sol::state& lua, const std::string& str, size_t& pos) {
            pos++; // Skip opening '{'
            sol::table tab = lua.create_table();
            while (pos < str.size()) {
                skipWhitespace(str, pos);
                if (pos >= str.size()) break;
                if (str[pos] == '}') {
                    pos++;
                    return tab;
                }
                if (str[pos] != '"') {
                    break;
                }
                std::string key = parseString(str, pos);
                skipWhitespace(str, pos);
                if (pos < str.size() && str[pos] == ':') {
                    pos++; // Skip colon
                }
                skipWhitespace(str, pos);
                sol::object val = parseValue(lua, str, pos);
                tab[key] = val;
                skipWhitespace(str, pos);
                if (pos < str.size() && str[pos] == ',') {
                    pos++;
                } else if (pos < str.size() && str[pos] == '}') {
                    pos++;
                    return tab;
                } else {
                    break;
                }
            }
            return tab;
        }

        sol::object parseArray(sol::state& lua, const std::string& str, size_t& pos) {
            pos++; // Skip opening '['
            sol::table tab = lua.create_table();
            size_t index = 1;
            while (pos < str.size()) {
                skipWhitespace(str, pos);
                if (pos >= str.size()) break;
                if (str[pos] == ']') {
                    pos++;
                    return tab;
                }
                sol::object val = parseValue(lua, str, pos);
                tab[index++] = val;
                skipWhitespace(str, pos);
                if (pos < str.size() && str[pos] == ',') {
                    pos++;
                } else if (pos < str.size() && str[pos] == ']') {
                    pos++;
                    return tab;
                } else {
                    break;
                }
            }
            return tab;
        }

        sol::object parseValue(sol::state& lua, const std::string& str, size_t& pos) {
            skipWhitespace(str, pos);
            if (pos >= str.size()) return sol::nil;

            char c = str[pos];
            if (c == '"') {
                return sol::make_object(lua, parseString(str, pos));
            } else if (c == '{') {
                return parseObject(lua, str, pos);
            } else if (c == '[') {
                return parseArray(lua, str, pos);
            } else if (c == 't' && pos + 3 < str.size() && str.substr(pos, 4) == "true") {
                pos += 4;
                return sol::make_object(lua, true);
            } else if (c == 'f' && pos + 4 < str.size() && str.substr(pos, 5) == "false") {
                pos += 5;
                return sol::make_object(lua, false);
            } else if (c == 'n' && pos + 3 < str.size() && str.substr(pos, 4) == "null") {
                pos += 4;
                return sol::nil;
            } else {
                // Parse numerical value
                size_t start = pos;
                if (str[pos] == '-') pos++;
                while (pos < str.size() && (std::isdigit(static_cast<unsigned char>(str[pos])) || str[pos] == '.' || str[pos] == 'e' || str[pos] == 'E' || str[pos] == '+' || str[pos] == '-')) {
                    pos++;
                }
                std::string numStr = str.substr(start, pos - start);
                try {
                    double val = std::stod(numStr);
                    return sol::make_object(lua, val);
                } catch (...) {
                    return sol::nil;
                }
            }
        }

        sol::object queryField(sol::object obj, const std::string& fieldPath) {
            if (!obj.valid() || obj.is<sol::nil_t>()) return sol::nil;

            std::stringstream ss(fieldPath);
            std::string part;
            sol::object current = obj;

            while (std::getline(ss, part, '.')) {
                if (!current.is<sol::table>()) return sol::nil;
                sol::table tab = current.as<sol::table>();
                current = tab[part];
                if (!current.valid() || current.is<sol::nil_t>()) return sol::nil;
            }
            return current;
        }
    }

    sol::object parseJSON_E(sol::state& lua, const std::string& jsonStr, sol::optional<std::string> fieldPath) {
        size_t pos = 0;
        sol::object obj = parseValue(lua, jsonStr, pos);
        if (fieldPath.has_value() && !fieldPath.value().empty()) {
            return queryField(obj, fieldPath.value());
        }
        return obj;
    }
}
