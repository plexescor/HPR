#pragma once

#include <variant>
#include <functional>
#include <map>
#include <vector>
#include <mutex>
#include <string>

//Some info for others
//If you want events which also include a data, use a struct EventName {data}
//And add the struct in the EventData's std::variant<EvenName>

// ---------- Data For Specific Events -------------------------
    struct Empty {};

    //Give the database a specific date to load data from
    struct DatabaseDate_Singular{
        std::string date;
    };

    //Data for window change event
    struct WindowChangedData {
        std::string fromWindow;
        std::string toWindow;
    };

    //An Error message
    struct ErrorGui{
        std::string error;
    };

    //Generic value representation to transfer dynamic structures between Lua and C++
    struct CppValue
    {
        enum class Type { Null, String, Double, Bool, Array, Struct };
        Type type = Type::Null;
        
        std::string str_val;
        double double_val = 0.0;
        bool bool_val = false;
        
        std::vector<CppValue> array_val;
        std::map<std::string, CppValue> struct_val;

        CppValue() = default;
        CppValue(Type t) : type(t) {}
        CppValue(Type t, std::string s) : type(t), str_val(s) {}
        CppValue(Type t, double d) : type(t), double_val(d) {}
        CppValue(Type t, bool b) : type(t), bool_val(b) {}
    };

// ------------    Actual Events -------------------------------
    enum class Event {
        LOAD_DATABASE_SINGULAR,
        HISTORY_LOADED_SINGULAR,
        LOAD_LIVE_DATA,
        APP_ERROR, //ERROR was reserved in msvc thats why
        MIDNIGHT_ROLLOVER,
        WINDOW_CHANGED,
        UI_READY
    };

using EventKey = std::variant<Event, std::string>;

using EventData = std::variant<Empty, DatabaseDate_Singular, ErrorGui, WindowChangedData, CppValue>;

// Converts type-safe EventData variant into generic CppValue
inline CppValue toCppValue(const EventData& data)
{
    CppValue result;
    std::visit([&](auto&& arg) 
    {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Empty>) 
        {
            result.type = CppValue::Type::Null;
        } 
        else if constexpr (std::is_same_v<T, DatabaseDate_Singular>) 
        {
            result.type = CppValue::Type::Struct;
            result.struct_val["date"] = CppValue(CppValue::Type::String, arg.date);
        } 
        else if constexpr (std::is_same_v<T, ErrorGui>) 
        {
            result.type = CppValue::Type::Struct;
            result.struct_val["error"] = CppValue(CppValue::Type::String, arg.error);
        } 
        else if constexpr (std::is_same_v<T, CppValue>) 
        {
            result = arg;
        }
        else if constexpr (std::is_same_v<T, WindowChangedData>) 
        {
            result.type = CppValue::Type::Struct;
            result.struct_val["fromWindow"] = CppValue(CppValue::Type::String, arg.fromWindow);
            result.struct_val["toWindow"] = CppValue(CppValue::Type::String, arg.toWindow);
        }
    }, data);
    return result;
}

// Converts generic CppValue back to specific EventData based on target event key
inline EventData toEventData(const EventKey& key, const CppValue& val)
{
    if (std::holds_alternative<Event>(key))
    {
        Event ev = std::get<Event>(key);
        if (ev == Event::LOAD_DATABASE_SINGULAR)
        {
            DatabaseDate_Singular d;
            if (val.type == CppValue::Type::Struct && val.struct_val.count("date"))
                d.date = val.struct_val.at("date").str_val;
            return d;
        }
        else if (ev == Event::APP_ERROR)
        {
            ErrorGui e;
            if (val.type == CppValue::Type::Struct && val.struct_val.count("error"))
                e.error = val.struct_val.at("error").str_val;
            return e;
        }
        else if (ev == Event::WINDOW_CHANGED)
        {
            WindowChangedData w;
            if (val.type == CppValue::Type::Struct && val.struct_val.count("fromWindow") && val.struct_val.count("toWindow"))
            {
                w.fromWindow = val.struct_val.at("fromWindow").str_val;
                w.toWindow = val.struct_val.at("toWindow").str_val;
            }
            return w;
        }
    }
    return val;
}

class EventHub
{
    //Gosh this shit is complex, i still can;t fully wrap my brain around it.
    public:

        //Things can connect to this in their own constructer to receive events and 
        //potentially some other data
        static size_t connect(EventKey event, std::function<void(EventData)> callback) 
        {
            std::lock_guard<std::mutex> lock(hubMutex);
            static size_t nextId = 0;
            size_t id = nextId++;
            subscribers[event][id] = callback;
            return id;
        }

        //Disconnect
        static void disconnect(EventKey event, size_t id)
        {
            std::lock_guard<std::mutex> lock(hubMutex);
            if (subscribers.count(event)) 
            {
                subscribers[event].erase(id);
            }
        }

        //Things can call specific Event and pass-in specific data so the signal can
        //be emitted to the listeners
        static void emit(EventKey event, EventData data = {})
        {

            std::map<EventKey, std::map<size_t, std::function<void(EventData)>>> subscribersLocal;
            
            {
                std::lock_guard<std::mutex> lock(hubMutex);
                subscribersLocal = subscribers;
            }
            
            if (subscribersLocal.count(event)) 
            {
                //iterate through the map of IDs and Callbacks
                for (auto const& [id, callback] : subscribersLocal[event]) 
                {
                    callback(data);
                }
            }
        }

    private:
        //Dont take risks
        static inline std::mutex hubMutex;
        static inline std::map<EventKey, std::map<size_t, std::function<void(EventData)>>> subscribers;
};