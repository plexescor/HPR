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

#include <slint-interpreter.h>

#include "sol.hpp"

#include "appState.hpp"
#include "uiRegistry.hpp"
#include "extensionManager.hpp"
#include "timeUtils.hpp"
#include "window_E.hpp"
#include "windowUtilities.hpp"
#include "windowBackendRegistery.hpp"
#include "appEvents.hpp"

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


ExtensionManager::ExtensionManager(DatabaseManager& dbm, bool allowDynamicLibraryExtensions) : dbManager(dbm), allowDynamicLibraryExtensionLoading(allowDynamicLibraryExtensions)
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
            ext->thread.join();
    }
    for (auto& ext : nativeExtensions)
    {
        if (ext->thread.joinable())
            ext->thread.join();
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
            
            registerFunctions(ext->lua);

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
}

void ExtensionManager::runExtension(LoadedExtension& ext)
{
    int sleepTime = 1000; //ms
    sol::function init = ext.lua["init"];
    sol::function onTick = ext.lua["onTick"];
    sol::function onExit = ext.lua["onExit"];

    if (init.valid()) sleepTime = init();
    
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
}

void ExtensionManager::registerFunctions(sol::state& lua)
{
    //Functions exposed to lua
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);
    
    lua["HPR"] = lua.create_table();
    
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

    lua["HPR"]["getAlias"] = [](std::string command) 
    {
        return AppState::aliasManager.getAlias(command);
    };

    lua["HPR"]["getAlias_Tab"] = [](std::string command) 
    {
        return AppState::aliasManager.getAlias_Tab(command);
    };

    lua["HPR"]["getAlias_Project"] = [](std::string command) 
    {
        return AppState::aliasManager.getAlias_Project(command);
    };

    lua["HPR"]["getReverseAlias"] = [](std::string aliasName) 
    {
        return AppState::aliasManager.getReverseAlias(aliasName);
    };

    lua["HPR"]["getReverseAlias_Tab"] = [](std::string aliasName) 
    {
        return AppState::aliasManager.getReverseAlias_Tab(aliasName);
    };

    lua["HPR"]["getReverseAlias_Project"] = [](std::string aliasName) 
    {
        return AppState::aliasManager.getReverseAlias_Project(aliasName);
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
        dbManager.executeSQL(sql, params.value_or(std::vector<std::string>{}));
    };

    lua["HPR"]["dbQuery_E"] = [this](std::string sql, sol::optional<std::vector<std::string>> params) 
    {
        return dbManager.querySQL(sql, params.value_or(std::vector<std::string>{}));
    };

    lua["HPR"]["getLoadedHistDbPath_E"] = [this]() 
    {
        return dbManager.getLoadedHistDbPath();
    };

    lua["HPR"]["dbQueryHistorical_E"] = [this](std::string sql, sol::optional<std::vector<std::string>> params) 
    {
        std::string histPath = dbManager.getLoadedHistDbPath();
        if (histPath.empty()) 
        {
            return std::vector<std::map<std::string, std::string>>{};
        }
        return dbManager.querySQL_Path(histPath, sql, params.value_or(std::vector<std::string>{}));
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

    lua["HPR"]["registerUiCallback"] = [](std::string name, sol::function luaCallback) 
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
        else eventKey = eventName;

        // Convert Lua parameter to generic CppValue, then map to specific C++ EventData structure
        CppValue cppVal = luaData.has_value() ? luaToCpp(luaData.value()) : CppValue();
        EventData data = toEventData(eventKey, cppVal);

        EventHub::emit(eventKey, data);
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