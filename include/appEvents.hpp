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

    //An Error message
    struct ErrorGui{
        std::string error;
    };

// ------------    Actual Events -------------------------------
    enum class Event {
        LOAD_DATABASE_SINGULAR,
        HISTORY_LOADED_SINGULAR,
        LOAD_LIVE_DATA,
        APP_ERROR //ERROR was reserved in msvc thats why
    };

using EventData = std::variant<Empty, DatabaseDate_Singular, ErrorGui>;

class EventHub
{
    //Gosh this shit is complex, i still can;t fully wrap my brain around it.
    public:

        //Things can connect to this in their own constructer to receive events and 
        //potentially some other data
        static size_t connect(Event event, std::function<void(EventData)> callback) 
        {
            std::lock_guard<std::mutex> lock(hubMutex);
            static size_t nextId = 0;
            size_t id = nextId++;
            subscribers[event][id] = callback;
            return id;
        }

        //Disconnect
        static void disconnect(Event event, size_t id) 
        {
            std::lock_guard<std::mutex> lock(hubMutex);
            if (subscribers.count(event)) {
                subscribers[event].erase(id);
            }
        }

        //Things can call specific Event and pass-in specific data so the signal can
        //be emitted to the listeners
        static void emit(Event event, EventData data = {}) 
        {

            std::map<Event, std::map<size_t, std::function<void(EventData)>>> subscribersLocal;
            
            {
                std::lock_guard<std::mutex> lock(hubMutex);
                subscribersLocal = subscribers;
            }
            
            if (subscribersLocal.count(event)) 
            {
                //iterate through the map of IDs and Callbacks
                for (auto const& [id, callback] : subscribersLocal[event]) {
                    callback(data);
                }
            }
        }

    private:
        //Dont take risks
        static inline std::mutex hubMutex;
        static inline std::map<Event, std::map<size_t, std::function<void(EventData)>>> subscribers;
};