#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex> 
#include <functional>
#include <map>

#ifdef __linux__
    #include <unistd.h>
#endif

#include <sstream>


#include <slint-interpreter.h>

#include "sol.hpp"

#include "appState.hpp"
#include "uiRegistry.hpp"
#include "getCurrentWindow.hpp"
#include "extensionManager.hpp"
#include "timeUtils.hpp"
#include "window_E.hpp"
#include "windowUtilities.hpp"
#include "windowBackendRegistery.hpp"
#include "appEvents.hpp"
#include "jsonUtilities.hpp"
#include "netUtilities.hpp"

#ifdef _WIN32
    #include <windows.h>
    std::vector<HMODULE> nativeHandles;
#else
    #include <dlfcn.h>
    std::vector<void*> nativeHandles;
#endif

namespace
{
    CppValue luaToCpp(const sol::object& obj)
    {
        CppValue v;
        if (obj.is<std::string>())
        {
            v.type = CppValue::Type::String;
            v.str_val = obj.as<std::string>();
        }
        else if (obj.is<double>())
        {
            v.type = CppValue::Type::Double;
            v.double_val = obj.as<double>();
        }
        else if (obj.is<bool>())
        {
            v.type = CppValue::Type::Bool;
            v.bool_val = obj.as<bool>();
        }
        else if (obj.is<sol::table>())
        {
            sol::table tab = obj.as<sol::table>();
            bool is_array = false;
            if (tab[1].valid())
            {
                is_array = true;
            }
            else
            {
                // Default empty tables to Array so they clear list models in Slint
                bool has_keys = false;
                tab.for_each([&](sol::object key, sol::object value) {
                    has_keys = true;
                });
                if (!has_keys)
                {
                    is_array = true;
                }
            }
            
            if (is_array)
            {
                v.type = CppValue::Type::Array;
                for (size_t i = 1; ; ++i)
                {
                    sol::object val = tab[i];
                    if (!val.valid() || val.is<sol::nil_t>())
                    {
                        break;
                    }
                    v.array_val.push_back(luaToCpp(val));
                }
            }
            else
            {
                v.type = CppValue::Type::Struct;
                tab.for_each([&](sol::object key, sol::object value)
                {
                    if (key.is<std::string>())
                    {
                        v.struct_val[key.as<std::string>()] = luaToCpp(value);
                    }
                });
            }
        }
        return v;
    }

    slint::interpreter::Value cppToSlint(const CppValue& val)
    {
        if (val.type == CppValue::Type::String)
        {
            return slint::interpreter::Value(slint::SharedString(val.str_val));
        }
        else if (val.type == CppValue::Type::Double)
        {
            return slint::interpreter::Value(val.double_val);
        }
        else if (val.type == CppValue::Type::Bool)
        {
            return slint::interpreter::Value(val.bool_val);
        }
        else if (val.type == CppValue::Type::Array)
        {
            auto model = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
            for (const auto& item : val.array_val)
            {
                model->push_back(cppToSlint(item));
            }
            return slint::interpreter::Value(std::shared_ptr<slint::Model<slint::interpreter::Value>>(model));
        }
        else if (val.type == CppValue::Type::Struct)
        {
            slint::interpreter::Struct s;
            for (const auto& [k, v] : val.struct_val)
            {
                s.set_field(k, cppToSlint(v));
            }
            return slint::interpreter::Value(s);
        }
        return slint::interpreter::Value();
    }

    sol::object cppToLua(sol::state& lua, const CppValue& val)
    {
        if (val.type == CppValue::Type::String)
        {
            return sol::make_object(lua, val.str_val);
        }
        else if (val.type == CppValue::Type::Double)
        {
            return sol::make_object(lua, val.double_val);
        }
        else if (val.type == CppValue::Type::Bool)
        {
            return sol::make_object(lua, val.bool_val);
        }
        else if (val.type == CppValue::Type::Array)
        {
            sol::table tab = lua.create_table();
            for (size_t i = 0; i < val.array_val.size(); ++i)
            {
                tab[i + 1] = cppToLua(lua, val.array_val[i]);
            }
            return tab;
        }
        else if (val.type == CppValue::Type::Struct)
        {
            sol::table tab = lua.create_table();
            for (const auto& [k, v] : val.struct_val)
            {
                tab[k] = cppToLua(lua, v);
            }
            return tab;
        }
        return sol::nil;
    }
}


ExtensionManager::ExtensionManager(bool allowDynamicLibraryExtensions) : allowDynamicLibraryExtensionLoading(allowDynamicLibraryExtensions)
{
    allowDynamicLibraryExtensionLoading = allowDynamicLibraryExtensions;
    loadExtensions();
}

ExtensionManager::~ExtensionManager()
{
    //two passes for "fastness"
    for (auto& ext : extensions)
    {
        ext->running = false;
    }
    for (auto& ext : nativeExtensions)
    {
        ext->running = false;
    }

    for (auto& ext : extensions)
    {
        if (ext->thread.joinable())
        {
            // give it 200ms for extensino to exit otherwise
            // it will block the whole shutdown process, so we detach and let the OS clean it up
            auto future = std::async(std::launch::async, [&ext]() 
            {
                ext->thread.join();
            });
            if (future.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout)
            {
                std::cerr << "Extension " << ext->path << " timed out, detaching\n";
                ext->thread.detach(); 
                std::cerr << "Force exiting to avoid potential hangs\n";
                #ifdef _WIN32
                    TerminateProcess(GetCurrentProcess(), 0);
                #else
                    _exit(0);
                #endif
            }
        }
    }
    for (auto& ext : nativeExtensions)
    {
        if (ext->thread.joinable())
        {
            // give it 200ms for extensino to exit otherwise
            // it will block the whole shutdown process, so we detach and let the OS clean it
            auto future = std::async(std::launch::async, [&ext]() 
            {
                ext->thread.join();
            });

            if (future.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout)
            {
                std::cerr << "Extension " << ext->path << " timed out, detaching\n";
                ext->thread.detach();
                std::cerr << "Force exiting to avoid potential hangs\n";
                #ifdef _WIN32
                    TerminateProcess(GetCurrentProcess(), 0);
                #else
                    _exit(0);
                #endif
            }
        }
    
    }
    registeredBackends.clear();

    for (auto& ext : nativeExtensions)
    {
        if (ext->handle)
        {
            #ifdef _WIN32
                FreeLibrary(ext->handle);
            #else
                dlclose(ext->handle);
            #endif
        }
    }
}

void ExtensionManager::run()
{
    for (auto& ext : extensions)
    {
        ext->thread = std::thread(
            &ExtensionManager::runExtension,
            this,
            std::ref(*ext)
        );
    }
    for (auto& ext : nativeExtensions)
    {
        ext->thread = std::thread(
            &ExtensionManager::runNativeExtension,
            this,
            std::ref(*ext)
        );
    }
}

void ExtensionManager::loadExtensions()
{
    updateExtensionPath();

    try
    {

        if (allowDynamicLibraryExtensionLoading)
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(extensionPath))
            {
                #ifdef _WIN32
                if (entry.is_regular_file() && entry.path().extension() == ".dll")
                #else
                if (entry.is_regular_file() && (entry.path().extension() == ".so" || entry.path().extension() == ".dylib"))
                #endif
                {
                    loadNativeExtension(entry.path());
                }
            }
        }
        else
        {
            // check if any exist and warn
            for (const auto& entry : std::filesystem::recursive_directory_iterator(extensionPath))
            {
                #ifdef _WIN32
                bool isNative = entry.path().extension() == ".dll";
                #else
                bool isNative = (entry.path().extension() == ".so" || entry.path().extension() == ".dylib");
                #endif
                if (entry.is_regular_file() && isNative)
                {
                    std::cerr << "[HPR] Native extension found but allow-dynamic-library-extensions is false. Skipping: "
                            << entry.path() << '\n';
                }
            }
        }

        //load all lua files from this dir recursively
        for (const auto& entry : std::filesystem::recursive_directory_iterator(extensionPath))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".lua")
            {
                auto ext = std::make_unique<LoadedExtension>();
                ext->path = entry.path();
                
                registerFunctions(*ext);

                try
                {
                    ext->lua.script_file(entry.path().string());
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Failed to load extension: "
                            << entry.path()
                            << "\nError: " << e.what() << '\n';
                    continue;
                }
                            
                extensions.push_back(std::move(ext));
            }
            else if (entry.is_regular_file())
            {
                std::cerr << "Skipping non-lua file: " << entry.path() << '\n';
            }
        }
    } catch (std::exception& e)
    {
        std::cerr << "Error loading extensions: " << e.what() << '\n';
    }
}

void ExtensionManager::loadNativeExtension(const std::filesystem::path& path)
{
    
    auto ext = std::make_unique<NativeExtension>();
    ext->path = path;

    #ifdef _WIN32
        ext->handle = LoadLibraryA(path.string().c_str());
        if (!ext->handle) {
            std::cerr << "[HPR] Failed to load native extension: " << path << '\n';
            return;
        }
    #else
        ext->handle = dlopen(path.string().c_str(), RTLD_LAZY);
        if (!ext->handle) {
            std::cerr << "[HPR] Failed to load native extension: " << path << "\n" << dlerror() << '\n';
            return;
        }
    #endif

    nativeExtensions.push_back(std::move(ext));
}

void ExtensionManager::runNativeExtension(NativeExtension& ext)
{
    try
    {
        int sleepTime = 1000; //ms

        #ifdef _WIN32
            auto init_fn = (int(*)())GetProcAddress(ext.handle, "init");
            auto onTick_fn = (void(*)(float))GetProcAddress(ext.handle, "onTick");
            auto onExit_fn = (void(*)())GetProcAddress(ext.handle, "onExit");
        #else
            auto init_fn = (int(*)())dlsym(ext.handle, "init");
            auto onTick_fn = (void(*)(float))dlsym(ext.handle, "onTick");
            auto onExit_fn = (void(*)())dlsym(ext.handle, "onExit");
        #endif

        if (init_fn) 
        {
            try {
                sleepTime = init_fn();
            } catch (const std::exception& e) {
                std::cerr << "Extension error in init for " << ext.path << ": " << e.what() << '\n';
            } catch (...) {
                std::cerr << "Unknown extension error in init for " << ext.path << '\n';
            }
        }
        
        auto lastTime = std::chrono::high_resolution_clock::now();
        while (ext.running)
        {
            auto currentTime = std::chrono::high_resolution_clock::now();
            float delta = std::chrono::duration<float, std::milli>(currentTime - lastTime).count();
            lastTime = currentTime;

            if (onTick_fn)
            {
                try {
                    onTick_fn(delta);
                } catch (const std::exception& e) {
                    std::cerr << "Extension error in onTick for " << ext.path << ": " << e.what() << '\n';
                } catch (...) {
                    std::cerr << "Unknown extension error in onTick for " << ext.path << '\n';
                }
            }

            int slept = 0;
            while (ext.running && slept < sleepTime)
            {
                int chunk = (sleepTime - slept < 100) ? (sleepTime - slept) : 100;
                std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
                slept += chunk;
            }
        }

        if (onExit_fn)
        {
            try {
                onExit_fn();
            } catch (const std::exception& e) {
                std::cerr << "Extension error in onExit for " << ext.path << ": " << e.what() << '\n';
            } catch (...) {
                std::cerr << "Unknown extension error in onExit for " << ext.path << '\n';
            }
        }
    } catch (const std::exception& e)
    {
        std::cerr << "Extension error in " << ext.path << ": " << e.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "Unknown extension error in " << ext.path << '\n';
    }
}

void ExtensionManager::runExtension(LoadedExtension& ext)
{
    try
    {

        sol::optional<std::string> authorName = ext.lua["HPR"]["authorName"];
        sol::optional<std::string> extensionName = ext.lua["HPR"]["extensionName"];
        if (authorName.has_value() && extensionName.has_value())
        {
            ext.identity = { authorName.value(), extensionName.value() };
            std::lock_guard<std::mutex> lock(AppState::stateMutex);
            AppState::state.loadedExtensions.push_back(ext.identity);
        }
        
        int sleepTime = 1000; //ms
        sol::function init = ext.lua["init"];
        sol::function onTick = ext.lua["onTick"];
        sol::function onExit = ext.lua["onExit"];

        if (init.valid()) 
        {
            sol::object result = init();
            if (result.is<int>()) sleepTime = result.as<int>();
        }
        
        auto lastTime = std::chrono::high_resolution_clock::now();
        while (ext.running)
        {
            auto currentTime = std::chrono::high_resolution_clock::now();

            float delta = std::chrono::duration<float, std::milli>(
                            currentTime - lastTime
                        ).count();
                

            lastTime = currentTime;

            try
            {
                if (onTick.valid()) onTick(delta);          
            }
            catch (const std::exception& e)
            {
                std::cerr << "Extension error in "
                        << ext.path
                        << ": "
                        << e.what()
                        << '\n';
            }
            int slept = 0;
            while (ext.running && slept < sleepTime)
            {
                int chunk = (sleepTime - slept < 100) ? (sleepTime - slept) : 100;
                std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
                slept += chunk;
            }
        }

        if (onExit.valid())
        {
            try
            {
                onExit();
            }
            catch (const std::exception& e)
            {
                std::cerr << "Extension error in onExit for "
                        << ext.path
                        << ": "
                        << e.what()
                        << '\n';
            }
        }
    } catch (const std::exception& e)
    {
        std::cerr << "Extension error in "
                << ext.path
                << ": "
                << e.what()
                << '\n';
    }
    catch (...)
    {
        std::cerr << "Unknown extension error in "
                << ext.path
                << '\n';
    }
}

void ExtensionManager::registerFunctions(LoadedExtension& ext)
{
    sol::state& lua = ext.lua;
    //Functions exposed to lua
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);
    
    lua["HPR"] = lua.create_table();

    lua["HPR"]["startServer_E"] = [&ext](int port, sol::function handler) -> bool
    {
        return NativeNet::startHttpServer(port, handler, ext);
    };

    lua["HPR"]["getTime_MS_E"] = []() -> uint64_t
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    };

    // lua["HPR"]["isUiActive_E"] = []() -> bool
    // {
    //     return UiRegistry::isActive();
    // };

    lua["HPR"]["sleep_E"] = [&ext](int ms)
    {
        int slept = 0;
        while (slept < ms && ext.running)
        {
            int chunk = (ms - slept < 50) ? (ms - slept) : 50;
            std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
            slept += chunk;
        }
    };

    lua["HPR"]["parseISO8601_E"] = [](std::string str) -> uint64_t
    {
        int y = 0, m = 0, d = 0, h = 0, min = 0;
        float sec_fraction = 0.0f;
        int parsed = std::sscanf(str.c_str(), "%d-%d-%dT%d:%d:%f", &y, &m, &d, &h, &min, &sec_fraction);
        if (parsed < 5) return 0;
        
        int s = static_cast<int>(sec_fraction);
        int ms = static_cast<int>((sec_fraction - s) * 1000.0f + 0.5f);

        std::tm t = {};
        t.tm_year = y - 1900;
        t.tm_mon = m - 1;
        t.tm_mday = d;
        t.tm_hour = h;
        t.tm_min = min;
        t.tm_sec = s;
        t.tm_isdst = -1;

#ifdef _WIN32
        std::time_t epoch_sec = _mkgmtime(&t);
#else
        std::time_t epoch_sec = timegm(&t);
#endif
        if (epoch_sec == -1) return 0;

        return static_cast<uint64_t>(epoch_sec) * 1000 + ms;
    };

    lua["HPR"]["httpGet_E"] = [](std::string host, std::string path, sol::optional<bool> secure) -> std::tuple<std::string, int>
    {
        auto result = NativeNet::httpGet(host, path, secure.value_or(true));
        return std::make_tuple(result.first, result.second);
    };

    lua["HPR"]["httpPost_E"] = [](std::string host, std::string path, std::string body, sol::optional<bool> secure) -> std::tuple<std::string, int>
    {
        auto result = NativeNet::httpPost(host, path, body, secure.value_or(true));
        return std::make_tuple(result.first, result.second);
    };

    lua["HPR"]["parseJSON_E"] = [&lua](std::string jsonStr, sol::optional<std::string> fieldPath) 
    {
        return JsonParser::parseJSON_E(lua, jsonStr, fieldPath);
    };

    lua["HPR"]["toJSON_E"] = [](sol::object luaTable) 
    {
        return JsonParser::toJSON_E(luaTable);
    };
    
    lua["HPR"]["getCurrentWindow_E"] = []() 
    {
        return getCurrentWindow_E();
    };

    lua["HPR"]["getCurrentTitle_E"] = []() 
    {
        return getCurrentTitle_E();
    };

    lua["HPR"]["runSystemCommand_E"] = [](std::string command) 
    {
        return runSystemCommand(command);
    };

    lua["HPR"]["getAlias_E"] = [](std::string command) 
    {
        return AppState::aliasManager.getAlias(command);
    };

    lua["HPR"]["getAlias_Tab_E"] = [](std::string command) 
    {
        return AppState::aliasManager.getAlias_Tab(command);
    };

    lua["HPR"]["getAlias_Project_E"] = [](std::string command) 
    {
        return AppState::aliasManager.getAlias_Project(command);
    };

    lua["HPR"]["getReverseAlias_E"] = [](std::string aliasName) 
    {
        return AppState::aliasManager.getReverseAlias(aliasName);
    };

    lua["HPR"]["getReverseAlias_Tab_E"] = [](std::string aliasName) 
    {
        return AppState::aliasManager.getReverseAlias_Tab(aliasName);
    };

    lua["HPR"]["getReverseAlias_Project_E"] = [](std::string aliasName) 
    {
        return AppState::aliasManager.getReverseAlias_Project(aliasName);
    };

    lua["HPR"]["stopTracking_E"] = [this]()
    {
        currentWindowManager->stopTracking();
    };

    lua["HPR"]["startTracking_E"] = [this]()
    {
        currentWindowManager->startTracking();
    };

    lua["HPR"]["registerBackend_E"] = [](
        std::string name, 
        sol::function matchesEnvironment,
        sol::function initialize,
        sol::function isUsable,
        sol::function getCurrentWindow,
        sol::function getCurrentTitle
    ) 
    {
        registerBackend_E(name, matchesEnvironment, initialize, isUsable, getCurrentWindow, getCurrentTitle);
    };

    lua["HPR"]["dbExecute_E"] = [this](std::string sql, sol::optional<std::vector<std::string>> params) 
    {
        dbManager->executeSQL(sql, params.value_or(std::vector<std::string>{}));
    };

    lua["HPR"]["dbQuery_E"] = [this](std::string sql, sol::optional<std::vector<std::string>> params) 
    {
        return dbManager->querySQL(sql, params.value_or(std::vector<std::string>{}));
    };

    lua["HPR"]["getLoadedHistDbPath_E"] = [this]() 
    {
        return dbManager->getLoadedHistDbPath();
    };

    lua["HPR"]["dbQueryHistorical_E"] = [this](std::string sql, sol::optional<std::vector<std::string>> params) 
    {
        // Block until the async DB load has finished (or 5s timeout to avoid
        // hanging forever if, e.g., the file wasn't found and load errored out)
        {
            std::unique_lock<std::mutex> lk(AppState::historyLoadedMutex);
            AppState::historyLoadedCV.wait_for(lk, std::chrono::seconds(5), []() {
                return AppState::historicalData_State.isLoaded;
            });
        }

        std::string histPath = dbManager->getLoadedHistDbPath();
        if (histPath.empty()) 
        {
            return std::vector<std::map<std::string, std::string>>{};
        }
        return dbManager->querySQL_Path(histPath, sql, params.value_or(std::vector<std::string>{}));
    };

    lua["HPR"]["getDbPathForDate_E"] = [](std::string date) -> std::string
    {
        return DatabaseManager::getDbPathForDate(date);
    };

    lua["HPR"]["dbQueryPath_E"] = [this](std::string dbPath, std::string sql, sol::optional<std::vector<std::string>> params) 
    {
        return dbManager->querySQL_Path(dbPath, sql, params.value_or(std::vector<std::string>{}));
    };

    lua["HPR"]["convertToDate_DDMMYY_E"] = [](uint64_t ms)
    {
        return convertToDate_DDMMYY(ms);
    };

    lua["HPR"]["convertToDate_MMYY_E"] = [](uint64_t ms)
    {
        return convertToDate_MMYY(ms);
    };

    lua["HPR"]["convertToTime_HHMMSS_12_E"] = [](uint64_t ms)
    {
        return convertToTime_HHMMSS_12(ms);
    };

    lua["HPR"]["formatTime_HHMMSS_E"] = [](int ms)
    {
        return formatTime_HHMMSS(ms);
    };

    lua["HPR"]["parseDate_DDMMYY_E"] = [](std::string dateStr)
    {
        return parseDate_DDMMYY(dateStr);
    };

    lua["HPR"]["parseDate_MMYY_E"] = [](std::string dateStr)
    {
        return parseDate_MMYY(dateStr);
    };

    lua["HPR"]["extractMMYY_from_DDMMYY_E"] = [](std::string dateStr)
    {
        return extractMMYY_from_DDMMYY(dateStr);
    };

    lua["HPR"]["setUiProperty_E"] = [](std::string name, sol::object value) 
    {
        if (!UiRegistry::isActive())
        {
            return; // UI is not active/loaded yet
        }
        
        auto weak = UiRegistry::getInstance();
        //Safely convert Lua object to standard C++ structure on the Lua execution background thread
        CppValue cppVal = luaToCpp(value);

        // Dispatch to Slint's main event loop thread to construct and set the Slint values
        slint::invoke_from_event_loop([weak, name, cppVal]() 
        {
            if (auto handle = weak.lock())
            {
                slint::interpreter::Value slintVal = cppToSlint(cppVal);
                (*handle)->set_property(name, slintVal);
            }
        });
    };

    lua["HPR"]["registerUiCallback_E"] = [](std::string name, sol::function luaCallback) 
    {
        if (!UiRegistry::isActive())
        {
            return; // no ui
        }
        
        auto weak = UiRegistry::getInstance();
        slint::invoke_from_event_loop([weak, name, luaCallback]() 
        {
            if (auto handle = weak.lock())
            {
                // lol auto vro iwill be cooked by code reviewers
                (*handle)->set_callback(name, [luaCallback](auto args) -> slint::interpreter::Value 
                {
                    // Trigger the lua function when Slint fires the callback
                    luaCallback();
                    return slint::interpreter::Value(); // void return
                });
            }
        });
    };

    // Connect to system or custom dynamic events
    lua["HPR"]["connect_E"] = [&lua](std::string eventName, sol::function callback) -> size_t 
    {
        EventKey eventKey;
        if (eventName == "LOAD_DATABASE_SINGULAR") eventKey = Event::LOAD_DATABASE_SINGULAR;
        else if (eventName == "HISTORY_LOADED_SINGULAR") eventKey = Event::HISTORY_LOADED_SINGULAR;
        else if (eventName == "LOAD_LIVE_DATA") eventKey = Event::LOAD_LIVE_DATA;
        else if (eventName == "APP_ERROR") eventKey = Event::APP_ERROR;
        else if (eventName == "MIDNIGHT_ROLLOVER") eventKey = Event::MIDNIGHT_ROLLOVER;
        else if (eventName == "WINDOW_CHANGED") eventKey = Event::WINDOW_CHANGED;
        else eventKey = eventName; // Custom signal

        return EventHub::connect(eventKey, [callback, &lua](EventData data) 
        {
            // Convert standard EventData to generic CppValue, then convert to native Lua value
            sol::object luaData = cppToLua(lua, toCppValue(data));
            callback(luaData);
        });
    };

    // Unsubscribe from a registered event
    lua["HPR"]["disconnect_E"] = [](std::string eventName, size_t id) 
    {
        EventKey eventKey;
        if (eventName == "LOAD_DATABASE_SINGULAR") eventKey = Event::LOAD_DATABASE_SINGULAR;
        else if (eventName == "HISTORY_LOADED_SINGULAR") eventKey = Event::HISTORY_LOADED_SINGULAR;
        else if (eventName == "LOAD_LIVE_DATA") eventKey = Event::LOAD_LIVE_DATA;
        else if (eventName == "APP_ERROR") eventKey = Event::APP_ERROR;
        else if (eventName == "MIDNIGHT_ROLLOVER") eventKey = Event::MIDNIGHT_ROLLOVER;
        else if (eventName == "WINDOW_CHANGED") eventKey = Event::WINDOW_CHANGED;
        else eventKey = eventName;

        EventHub::disconnect(eventKey, id);
    };

    // Emit system or custom dynamic events
    lua["HPR"]["emit_E"] = [](std::string eventName, sol::optional<sol::object> luaData) 
    {
        EventKey eventKey;
        if (eventName == "LOAD_DATABASE_SINGULAR") eventKey = Event::LOAD_DATABASE_SINGULAR;
        else if (eventName == "HISTORY_LOADED_SINGULAR") eventKey = Event::HISTORY_LOADED_SINGULAR;
        else if (eventName == "LOAD_LIVE_DATA") eventKey = Event::LOAD_LIVE_DATA;
        else if (eventName == "APP_ERROR") eventKey = Event::APP_ERROR;
        else if (eventName == "MIDNIGHT_ROLLOVER") eventKey = Event::MIDNIGHT_ROLLOVER;
        else if (eventName == "WINDOW_CHANGED") eventKey = Event::WINDOW_CHANGED;
        else eventKey = eventName;

        // Convert Lua parameter to generic CppValue, then map to specific C++ EventData structure
        CppValue cppVal = luaData.has_value() ? luaToCpp(luaData.value()) : CppValue();
        EventData data = toEventData(eventKey, cppVal);

        EventHub::emit(eventKey, data);
    };

    auto trim = [](const std::string& str) -> std::string 
    {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, last - first + 1);
    };

    auto parseValue = [trim](const std::string& rawValue, sol::state& luaState) -> sol::object 
    {
        std::string trimmed = trim(rawValue);
        if (trimmed == "true") {
            return sol::make_object(luaState, true);
        }
        if (trimmed == "false") {
            return sol::make_object(luaState, false);
        }
        if (!trimmed.empty()) {
            char* endptr = nullptr;
            double d = std::strtod(trimmed.c_str(), &endptr);
            if (endptr == trimmed.c_str() + trimmed.size()) {
                return sol::make_object(luaState, d);
            }
        }
        return sol::make_object(luaState, rawValue);
    };

    lua["HPR"]["readCsv_E"] = [this, &ext, &lua, trim, parseValue](std::string userPath, sol::optional<sol::object> keyOpt, sol::this_state ts) -> sol::variadic_results
    {
        sol::variadic_results vr;
        std::string err;
        std::filesystem::path securedPath = resolveAndSecurePath(userPath, this->extensionPath, err);
        if (securedPath.empty())
        {
            std::cerr << "[HPR Extension CSV Error] " << err << " (path: " << userPath << ")" << std::endl;
            vr.push_back(sol::make_object(ts, ""));
            return vr;
        }

        std::ifstream file(securedPath);
        if (!file.is_open())
        {
            std::cerr << "[HPR Extension CSV Error] Failed to open file for reading: " << securedPath << std::endl;
            vr.push_back(sol::make_object(ts, ""));
            return vr;
        }

        std::vector<std::string> keys;
        std::vector<std::string> values;
        std::string line;
        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\n') line.pop_back();
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            size_t commaPos = line.find(',');
            if (commaPos == std::string::npos)
            {
                keys.push_back(line);
                values.push_back("");
            }
            else
            {
                keys.push_back(line.substr(0, commaPos));
                values.push_back(line.substr(commaPos + 1));
            }
        }
        file.close();

        if (keyOpt.has_value() && !keyOpt.value().is<sol::nil_t>())
        {
            std::string targetKey;
            sol::object kObj = keyOpt.value();
            if (kObj.is<std::string>()) targetKey = kObj.as<std::string>();
            else if (kObj.is<double>()) {
                std::ostringstream oss;
                oss << kObj.as<double>();
                targetKey = oss.str();
            }
            else if (kObj.is<bool>()) targetKey = kObj.as<bool>() ? "true" : "false";

            for (size_t i = 0; i < keys.size(); ++i)
            {
                if (keys[i] == targetKey)
                {
                    vr.push_back(parseValue(values[i], lua));
                    return vr;
                }
            }
            vr.push_back(sol::make_object(ts, ""));
            return vr;
        }
        else
        {
            sol::table resultTable = lua.create_table();
            for (size_t i = 0; i < keys.size(); ++i)
            {
                resultTable[keys[i]] = parseValue(values[i], lua);
            }
            vr.push_back(resultTable);
            return vr;
        }
    };

    lua["HPR"]["writeCsv_E"] = [this, &ext, &lua](std::string userPath, sol::object key, sol::object value) -> bool
    {
        std::string err;
        std::filesystem::path securedPath = resolveAndSecurePath(userPath, this->extensionPath, err);
        if (securedPath.empty())
        {
            std::cerr << "[HPR Extension CSV Error] " << err << " (path: " << userPath << ")" << std::endl;
            return false;
        }

        std::string keyStr;
        if (key.is<std::string>()) keyStr = key.as<std::string>();
        else if (key.is<double>()) {
            std::ostringstream oss;
            oss << key.as<double>();
            keyStr = oss.str();
        }
        else if (key.is<bool>()) keyStr = key.as<bool>() ? "true" : "false";
        else return false;

        std::string valStr;
        if (value.is<std::string>()) valStr = value.as<std::string>();
        else if (value.is<double>()) {
            std::ostringstream oss;
            oss << value.as<double>();
            valStr = oss.str();
        }
        else if (value.is<bool>()) valStr = value.as<bool>() ? "true" : "false";
        else return false;

        try {
            std::filesystem::create_directories(securedPath.parent_path());
        } catch (const std::exception& e) {
            std::cerr << "[HPR Extension CSV Error] Failed to create directories for: " << securedPath << " (" << e.what() << ")" << std::endl;
            return false;
        }

        std::vector<std::pair<std::string, std::string>> entries;
        bool updated = false;

        std::ifstream inFile(securedPath);
        if (inFile.is_open())
        {
            std::string line;
            while (std::getline(inFile, line))
            {
                if (!line.empty() && line.back() == '\n') line.pop_back();
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) continue;

                size_t commaPos = line.find(',');
                if (commaPos == std::string::npos)
                {
                    entries.push_back({line, ""});
                }
                else
                {
                    entries.push_back({line.substr(0, commaPos), line.substr(commaPos + 1)});
                }
            }
            inFile.close();
        }

        for (auto& entry : entries)
        {
            if (entry.first == keyStr)
            {
                entry.second = valStr;
                updated = true;
                break;
            }
        }

        if (!updated)
        {
            entries.push_back({keyStr, valStr});
        }

        std::ofstream outFile(securedPath, std::ios::trunc);
        if (!outFile.is_open())
        {
            std::cerr << "[HPR Extension CSV Error] Failed to open file for writing: " << securedPath << std::endl;
            return false;
        }

        for (const auto& entry : entries)
        {
            outFile << entry.first << "," << entry.second << "\n";
        }
        outFile.close();

        return true;
    };

    lua["HPR"]["deleteCsv_E"] = [this, &ext](std::string userPath) -> bool
    {
        std::string err;
        std::filesystem::path securedPath = resolveAndSecurePath(userPath, this->extensionPath, err);
        if (securedPath.empty())
        {
            std::cerr << "[HPR Extension CSV Error] " << err << " (path: " << userPath << ")" << std::endl;
            return false;
        }

        try {
            if (std::filesystem::exists(securedPath)) {
                return std::filesystem::remove(securedPath);
            }
            return false;
        } catch (const std::exception& e) {
            std::cerr << "[HPR Extension CSV Error] Failed to delete file: " << securedPath << " (" << e.what() << ")" << std::endl;
            return false;
        }
    };

    lua["HPR"]["generateInsights_E"] = []()
    {
        AppState::patternAnalyzer.generateInsights();
    };

    lua["HPR"]["getMostUsed_E"] = []() -> std::string
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        return AppState::patternAnalyzer.getMostUsed();
    };

    lua["HPR"]["getTotalTrackedTime_E"] = []() -> std::string
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        return AppState::patternAnalyzer.getTotalTrackedTime();
    };

    lua["HPR"]["getSwitchCount_E"] = []() -> std::string
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        return AppState::patternAnalyzer.getSwitchCount();
    };

    lua["HPR"]["getMostSwitchedFrom_E"] = []() -> std::string
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        return AppState::patternAnalyzer.getMostSwitchedFrom();
    };

    lua["HPR"]["getMostSwitchedTo_E"] = []() -> std::string
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        return AppState::patternAnalyzer.getMostSwitchedTo();
    };

    lua["HPR"]["getMostFocusedSession_E"] = []() -> std::string
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        return AppState::patternAnalyzer.getMostFocusedSession();
    };

    lua["HPR"]["getMostProductiveHour_E"] = []() -> std::string
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        return AppState::patternAnalyzer.getMostProductiveHour();
    };

    lua["HPR"]["showUi_E"] = [this]()
    {
        if (app) app->show();
        else if (interpreterApp) interpreterApp->show();
    };

    lua["HPR"]["hideUi_E"] = [this]()
    {
        if (app) app->hide();
        else if (interpreterApp) interpreterApp->hide();
    };

    lua["HPR"]["quitUi_E"] = [this]()
    {
        if (app) app->quit();
        else if (interpreterApp) interpreterApp->quit();
    };

    lua["HPR"]["getOsName_E"] = []() -> std::string
    {
        #ifdef _WIN32
            return "Windows";
        #elif defined(__APPLE__)
            return "Apple";
        #else
            return "Linux";
        #endif
    };

    lua["HPR"]["getEnvironmentName_E"] = []() -> std::string
    {
        #ifdef _WIN32
            return "";
        #else
            std::lock_guard<std::mutex> lock(AppState::stateMutex);
            return AppState::state.currentPlatform;
        #endif
    };

    lua["HPR"]["getLoadedExtensions_E"] = [&lua]() -> sol::table
    {
        std::lock_guard<std::mutex> lock(AppState::stateMutex);
        sol::table listTable = lua.create_table();
        int index = 1;
        for (const auto& identity : AppState::state.loadedExtensions)
        {
            sol::table entryTable = lua.create_table();
            entryTable["authorName"] = identity.first;
            entryTable["extensionName"] = identity.second;
            listTable[index++] = entryTable;
        }
        return listTable;
    };

    lua["HPR"]["crash_E"] = [](sol::optional<std::string> message)
    {
        if (message.has_value())
        {
            std::cout << "[HPR Extension Crash] " << message.value() << std::endl;
            std::cerr << "[HPR Extension Crash] " << message.value() << std::endl;
        }
        else
        {
            std::cout << "[HPR Extension Crash] HPR has been intentionally crashed via HPR.crash_E() by an extension!" << std::endl;
            std::cerr << "[HPR Extension Crash] HPR has been intentionally crashed via HPR.crash_E() by an extension!" << std::endl;
        }
        std::exit(1);
    };

    lua["HPR"]["getLiveTimeLogPerApp_E"] = [&lua]() -> sol::table
    {
        std::map<std::string, long> copy;
        {
            std::lock_guard<std::mutex> lock(AppState::stateMutex);
            copy = AppState::state.timeLog_PerApp;
        }
        sol::table result = lua.create_table();
        for (const auto& [app, ms] : copy)
        {
            result[app] = ms;
        }
        return result;
    };

    lua["HPR"]["getLiveTimeLogPerTab_E"] = [&lua]() -> sol::table
    {
        std::map<std::string, long> copy;
        {
            std::lock_guard<std::mutex> lock(AppState::stateMutex);
            copy = AppState::state.timeLog_PerTab;
        }
        sol::table result = lua.create_table();
        for (const auto& [tab, ms] : copy)
        {
            result[tab] = ms;
        }
        return result;
    };

    lua["HPR"]["getLiveTimeLogPerProject_E"] = [&lua]() -> sol::table
    {
        std::map<std::string, long> copy;
        {
            std::lock_guard<std::mutex> lock(AppState::stateMutex);
            copy = AppState::state.timeLog_PerProject;
        }
        sol::table result = lua.create_table();
        for (const auto& [project, ms] : copy)
        {
            result[project] = ms;
        }
        return result;
    };

}

void ExtensionManager::updateExtensionPath()
{
    #ifdef _WIN32
        extensionPath = std::getenv("APPDATA");
        extensionPath += "/HPR/HPR_Config/extensions/";
    #else
        const char* home = std::getenv("HOME");
        if (!home) throw std::runtime_error("HOME env var not set");
        extensionPath = home;
        extensionPath += "/.config/HPR/extensions/";
    #endif

    std::filesystem::create_directories(extensionPath);
}

std::filesystem::path resolveAndSecurePath(const std::string& userPath, const std::filesystem::path& baseDir, std::string& errOut)
{
    std::filesystem::path userP(userPath);
    if (userP.is_absolute() || userPath.rfind("/", 0) == 0 || userPath.rfind("\\", 0) == 0 || (userPath.size() >= 2 && userPath[1] == ':'))
    {
        errOut = "Access denied: Absolute paths are not supported.";
        return {};
    }

    std::filesystem::path target = baseDir / userP;
    std::filesystem::path normalTarget = std::filesystem::weakly_canonical(target);
    std::filesystem::path canonicalBase = std::filesystem::weakly_canonical(baseDir);

    auto relative = std::filesystem::relative(normalTarget, canonicalBase);
    if (relative.empty() || relative.string() == "." || relative.string().find("..") != std::string::npos)
    {
        errOut = "Access denied: Path is outside the extensions directory.";
        return {};
    }
    return normalTarget;
}