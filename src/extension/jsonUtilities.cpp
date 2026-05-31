#include "jsonUtilities.hpp"
#include <sstream>
#include <cctype>
#include <iostream>
#include <unordered_set>

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

    // Escapes a raw string so it is safe to embed inside JSON double quotes
    static std::string jsonEscapeString(const std::string& s)
    {
        std::string out;
        out.reserve(s.size() + 2);
        for (unsigned char c : s)
        {
            switch (c)
            {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                case '\b': out += "\\b";  break;
                case '\f': out += "\\f";  break;
                default:
                    if (c < 0x20) {
                        // Control characters -> \uXXXX
                        char buf[7];
                        std::snprintf(buf, sizeof(buf), "\\u%04X", c);
                        out += buf;
                    } else {
                        out += static_cast<char>(c);
                    }
            }
        }
        return out;
    }

    static std::string toJSON_Internal(const sol::object& obj, std::unordered_set<const void*>& visited)
    {
        if (!obj.valid() || obj.is<sol::nil_t>())
        {
            return "null";
        }
        else if (obj.is<bool>())
        {
            return obj.as<bool>() ? "true" : "false";
        }
        else if (obj.is<double>())
        {
            // Format the number without unnecessary trailing zeros
            double val = obj.as<double>();
            // Check if value is an integer to avoid printing "1.00000" style output
            if (val == static_cast<long long>(val) && val >= -1e15 && val <= 1e15)
            {
                return std::to_string(static_cast<long long>(val));
            }
            // Floating point representation
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.17g", val);
            return buf;
        }
        else if (obj.is<std::string>())
        {
            return "\"" + jsonEscapeString(obj.as<std::string>()) + "\"";
        }
        else if (obj.is<sol::table>())
        {
            sol::table tab = obj.as<sol::table>();
            const void* tabPtr = tab.pointer();
            if (visited.find(tabPtr) != visited.end())
            {
                std::string warningMsg = "\n[HPR SECURITY ERROR] An active Lua extension attempted to trigger a stack overflow / crash "
                                         "by serializing a cyclic self-referential table (circular reference detected at address " + 
                                         std::to_string(reinterpret_cast<uintptr_t>(tabPtr)) + "). Serializing as null to prevent SegFault.\n";
                std::cerr << warningMsg;
                std::cout << warningMsg;
                return "null";
            }
            visited.insert(tabPtr);

            // Determine if this is a Lua array (consecutive integer keys starting at 1)
            // or an object (string-keyed table)
            bool isArray = false;
            size_t arrayLen = 0;
            sol::object firstEntry = tab[1];
            if (firstEntry.valid() && !firstEntry.is<sol::nil_t>())
            {
                // Verify that keys 1..N are all present with no gaps
                isArray = true;
                for (size_t i = 1; ; ++i)
                {
                    sol::object entry = tab[i];
                    if (!entry.valid() || entry.is<sol::nil_t>())
                    {
                        arrayLen = i - 1;
                        break;
                    }
                }
            }

            std::string result;
            if (isArray)
            {
                // Serialize as JSON array
                result = "[";
                for (size_t i = 1; i <= arrayLen; ++i)
                {
                    if (i > 1) result += ",";
                    result += toJSON_Internal(tab[i], visited);
                }
                result += "]";
            }
            else
            {
                // Serialize as JSON object (string-keyed pairs only)
                result = "{";
                bool first = true;
                tab.for_each([&](const sol::object& key, const sol::object& val)
                {
                    if (key.is<std::string>())
                    {
                        if (!first) result += ",";
                        result += "\"" + jsonEscapeString(key.as<std::string>()) + "\":";
                        result += toJSON_Internal(val, visited);
                        first = false;
                    }
                });
                result += "}";
            }
            visited.erase(tabPtr);
            return result;
        }

        // Fallback for unsupported Lua types (functions, userdata, threads)
        return "null";
    }

    std::string toJSON_E(const sol::object& obj)
    {
        std::unordered_set<const void*> visited;
        return toJSON_Internal(obj, visited);
    }
}

