#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "logger.hpp"
#ifdef __linux__
#include <unistd.h>
#endif

#include <sstream>

#include <slint-interpreter.h>

#include "sol.hpp"

#include "limitsManager.hpp"

#include "HPR.hpp"
#include "HPRInterpreter.hpp"
#include "appEvents.hpp"
#include "appState.hpp"
#include "currentWindowManager.hpp"
#include "extensionManager.hpp"
#include "jsonUtilities.hpp"
#include "netUtilities.hpp"
#include "timeUtils.hpp"
#include "uiRegistry.hpp"
#include "windowBackendRegistery.hpp"
#include "windowUtilities.hpp"
#include "window_E.hpp"

extern "C" HPR_API void HPR_invokeOnSlintThread(void(*fn)(void*), void* userdata)
{
    slint::invoke_from_event_loop([fn, userdata]()
    {
        fn(userdata);
    });
}

namespace
{
CppValue luaToCpp_Internal(const sol::object &obj, std::unordered_set<const void *> &visited)
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
		const void *tabPtr = tab.pointer();
		if (visited.find(tabPtr) != visited.end())
		{
			std::string extName = "Unknown Extension";
			sol::state_view lua(obj.lua_state());
			if (lua["HPR"].valid() && lua["HPR"]["extensionName"].valid())
			{
				extName = lua["HPR"]["extensionName"].get<std::string>();
			}
			std::string warningMsg = "\n[HPR SECURITY ERROR] Active Lua extension [" + extName +
									 "] attempted to trigger a stack overflow / crash "
									 "by pushing a cyclic self-referential table to the UI "
									 "(circular "
									 "reference detected at address " +
									 std::to_string(reinterpret_cast<uintptr_t>(tabPtr)) +
									 "). Returning empty object to prevent SegFault.\n";
			std::cerr << warningMsg;
			Logger::log(warningMsg);
			return v;
		}
		visited.insert(tabPtr);

		bool is_array = false;
		if (tab[1].valid())
		{
			is_array = true;
		}
		else
		{
			// Default empty tables to Array so they clear list models in Slint
			bool has_keys = false;
			tab.for_each([&](sol::object key, sol::object value) { has_keys = true; });
			if (!has_keys)
			{
				is_array = true;
			}
		}

		if (is_array)
		{
			v.type = CppValue::Type::Array;
			for (size_t i = 1;; ++i)
			{
				sol::object val = tab[i];
				if (!val.valid() || val.is<sol::nil_t>())
				{
					break;
				}
				v.array_val.push_back(luaToCpp_Internal(val, visited));
			}
		}
		else
		{
			v.type = CppValue::Type::Struct;
			tab.for_each(
				[&](sol::object key, sol::object value)
				{
					if (key.is<std::string>())
					{
						v.struct_val[key.as<std::string>()] = luaToCpp_Internal(value, visited);
					}
				});
		}
		visited.erase(tabPtr);
	}
	return v;
}

CppValue luaToCpp(const sol::object &obj)
{
	std::unordered_set<const void *> visited;
	return luaToCpp_Internal(obj, visited);
}

CppValue slintToCpp(const slint::interpreter::Value &val)
{
	CppValue v;
	if (auto s = val.to_string())
	{
		v.type = CppValue::Type::String;
		v.str_val = std::string(*s);
	}
	else if (auto n = val.to_number())
	{
		v.type = CppValue::Type::Double;
		v.double_val = *n;
	}
	else if (auto b = val.to_bool())
	{
		v.type = CppValue::Type::Bool;
		v.bool_val = *b;
	}
	else if (auto arr = val.to_array())
	{
		v.type = CppValue::Type::Array;
		for (size_t i = 0; i < arr->size(); ++i)
		{
			v.array_val.push_back(slintToCpp((*arr)[i]));
		}
	}
	else if (auto strct = val.to_struct())
	{
		v.type = CppValue::Type::Struct;
		for (const auto &[name, fieldVal] : *strct)
		{
			v.struct_val[std::string(name)] = slintToCpp(fieldVal);
		}
	}
	return v;
}

slint::interpreter::Value cppToSlint(const CppValue &val)
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
		for (const auto &item : val.array_val)
		{
			model->push_back(cppToSlint(item));
		}
		return slint::interpreter::Value(std::shared_ptr<slint::Model<slint::interpreter::Value>>(model));
	}
	else if (val.type == CppValue::Type::Struct)
	{
		slint::interpreter::Struct s;
		for (const auto &[k, v] : val.struct_val)
		{
			s.set_field(k, cppToSlint(v));
		}
		return slint::interpreter::Value(s);
	}
	return slint::interpreter::Value();
}

sol::object cppToLua(sol::state &lua, const CppValue &val)
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
		for (const auto &[k, v] : val.struct_val)
		{
			tab[k] = cppToLua(lua, v);
		}
		return tab;
	}
	return sol::nil;
}
} // namespace

LoadedExtension::~LoadedExtension()
{
	for (const auto &conn : registeredConnections)
	{
		EventHub::disconnect(conn.first, conn.second);
	}
}

ExtensionManager::ExtensionManager() { loadExtensions(); }

ExtensionManager::~ExtensionManager()
{
	if (currentWindowManager)
	{
		currentWindowManager->stopTracking();
	}
	unregisterNonNativeBackends();

	if (didTimeoutDuringUnload)
	{
		std::cerr << "[HPR] Warning one or more extensions timed out during "
					 "unload/reload, the app may freeze";
		Logger::log("[HPR] Warning one or more extensions timed out during "
					"unload/reload, the app may freeze");
	}
	app = nullptr; // null BEFORE joining threads
	interpreterApp = nullptr;

	// two passes for "fastness"
	for (auto &ext : extensions)
	{
		ext->running = false;
	}

	for (auto &ext : extensions)
	{
		if (ext->thread.joinable())
		{
			bool joined = false;
			std::thread joiner(
				[&]()
				{
					ext->thread.join();
					joined = true;
				});

			auto start = std::chrono::steady_clock::now();
			int shutdownTimeout = AppState::configManager.getConfig("extension-shutdown-timeout", 300);
			while (!joined)
			{
				if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
						.count() >= shutdownTimeout)
				{
					std::cerr << "Extension " << ext->path << " timed out, detaching\n";
					Logger::log("[HPR] Extension " + ext->path.string() +
								" timed out during shutdown, detaching thread "
								"to prevent hang");
					ext->thread.detach();
					joiner.detach();
					ext->detached = true;
					std::unique_lock<std::recursive_mutex> lock(ext->luaMutex, std::defer_lock);
					if (lock.try_lock())
					{
						ext->lua["HPR"] = sol::nil;
					}
					std::cerr << "Force exiting to avoid potential hangs\n";
					Logger::log("[HPR] Force exiting due to extension shutdown "
								"timeout");
#ifdef _WIN32
					TerminateProcess(GetCurrentProcess(), 0);
#else
					_exit(0);
#endif
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			if (joined)
			{
				joiner.join();
			}
		}
	}
	registeredBackends.clear();
}

void ExtensionManager::run()
{
	for (auto &ext : extensions)
	{
		ext->thread = std::thread(&ExtensionManager::runExtension, this, ext);
	}
}

void ExtensionManager::loadExtensions()
{
	updateExtensionPath();

	try
	{
		// load all lua files from this dir recursively
		for (const auto &entry : std::filesystem::recursive_directory_iterator(extensionPath))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".lua")
			{
				auto ext = std::make_shared<LoadedExtension>();
				ext->path = entry.path();

				// Pre-initialize Lua state so the script can execute and set config flags
				bool allowNativeLibraries = AppState::configManager.getConfig<bool>("allow-native-libraries", false);
				if (allowNativeLibraries)
				{
					ext->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math,
											sol::lib::package);
				}
				else
				{
					ext->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);
				}
				ext->lua["HPR"] = ext->lua.create_table();
				ext->lua["HPR"]["overrides"] = ext->lua.create_table();

				try
				{
					ext->lua.script_file(entry.path().string());
				}
				catch (const std::exception &e)
				{
					std::cerr << "Failed to load extension: " << entry.path() << "\nError: " << e.what() << '\n';
					Logger::log("Failed to load extension: " + entry.path().string() + "\nError: " + e.what());
					continue;
				}

				sol::optional<bool> useLegacyAPISuffix = ext->lua["HPR"]["useLegacyAPISuffix"];
				if (useLegacyAPISuffix.has_value())
				{
					ext->useLegacyAPISuffix = useLegacyAPISuffix.value();
				}

				sol::optional<bool> useHPRTablePrefix = ext->lua["HPR"]["useHPRTablePrefix"];
				if (useHPRTablePrefix.has_value())
				{
					ext->useHPRTablePrefix = useHPRTablePrefix.value();
				}

				registerFunctions(*ext);

				extensions.push_back(std::move(ext));
			}
			else if (entry.is_regular_file())
			{
				// std::cerr << "Skipping non-lua file: " << entry.path() << '\n';
				// Logger::log("Skipping non-lua file: " + entry.path().string());
			}
		}
	}
	catch (std::exception &e)
	{
		std::cerr << "Error loading extensions: " << e.what() << '\n';
		Logger::log("Error loading extensions: " + std::string(e.what()));
	}
}

void ExtensionManager::runExtension(std::shared_ptr<LoadedExtension> ext_ptr)
{
	auto &ext = *ext_ptr;
	try
	{

		sol::optional<std::string> authorName = ext.lua["HPR"]["authorName"];
		sol::optional<std::string> extensionName = ext.lua["HPR"]["extensionName"];

		sol::optional<bool> useLegacyAPISuffix = ext.lua["HPR"]["useLegacyAPISuffix"];

		// IF EXTENSION DOESNT HAVE a name or author name then make it random AF
		auto genRandom = []() -> std::string
		{
			static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKL"
										"MNOPQRSTUVWXYZ0123456789";
			std::string result;
			for (int i = 0; i < 14; ++i)
				result += chars[rand() % (sizeof(chars) - 1)];
			return result;
		};

		std::string resolvedAuthor = authorName.value_or("anon_" + genRandom());
		std::string resolvedName = extensionName.value_or("ext_" + genRandom());

		if (useLegacyAPISuffix.has_value())
		{
			ext.useLegacyAPISuffix = useLegacyAPISuffix.value();
		}

		sol::optional<bool> useHPRTablePrefix = ext.lua["HPR"]["useHPRTablePrefix"];
		if (useHPRTablePrefix.has_value())
		{
			ext.useHPRTablePrefix = useHPRTablePrefix.value();
		}

		ext.identity = {resolvedAuthor, resolvedName};

		sol::optional<sol::object> versionSupportObj = ext.lua["HPR"]["versionSupport"];
		std::vector<std::string> versionsVec;
		bool hasVersionSupport = false;

		if (versionSupportObj.has_value() && versionSupportObj.value().is<sol::table>())
		{
			hasVersionSupport = true;
			sol::table vTable = versionSupportObj.value().as<sol::table>();
			for (auto &pair : vTable)
			{
				if (pair.second.is<std::string>())
				{
					std::string vStr = pair.second.as<std::string>();
					if (!vStr.empty() && (vStr[0] == 'v' || vStr[0] == 'V'))
					{
						vStr = vStr.substr(1);
					}
					versionsVec.push_back(vStr);
				}
			}
		}

		ext.versionSupport = versionsVec;
		ext.hasVersionSupport = hasVersionSupport;

		std::string currentAppVer = AppState::APP_VERSION;
		if (!currentAppVer.empty() && (currentAppVer[0] == 'v' || currentAppVer[0] == 'V'))
		{
			currentAppVer = currentAppVer.substr(1);
		}

		if (!hasVersionSupport)
		{
			ext.isCompatible = false;
			ext.warningMessage = "Unknown version compatibility for HPR " + currentAppVer;
		}
		else
		{
			bool matchFound = false;
			for (const auto &v : versionsVec)
			{
				if (v == currentAppVer)
				{
					matchFound = true;
					break;
				}
			}
			if (matchFound)
			{
				ext.isCompatible = true;
				ext.warningMessage = "";
			}
			else
			{
				ext.isCompatible = false;
				std::string supportedStr;
				for (size_t i = 0; i < versionsVec.size(); ++i)
				{
					if (i > 0)
						supportedStr += ", ";
					supportedStr += versionsVec[i];
				}
				ext.warningMessage = "This extension is incompatible with HPR " + currentAppVer +
									 ", the versions supported by the extension are: (" + supportedStr + ")";
			}
		}

		AppState::ExtensionInfo info;
		info.author = resolvedAuthor;
		info.name = resolvedName;
		info.versionSupport = ext.versionSupport;
		info.hasVersionSupport = ext.hasVersionSupport;
		info.isCompatible = ext.isCompatible;
		info.warningMessage = ext.warningMessage;

		{
			std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
			AppState::state.loadedExtensions.push_back(ext.identity);
			AppState::state.extensionMap[ext.identity] = info;
		}
		int sleepTime = 1000; // ms
		sol::function init = ext.lua["init"];
		sol::function onTick = ext.lua["onTick"];
		sol::function onExit = ext.lua["onExit"];

		if (init.valid())
		{
			std::lock_guard<std::recursive_mutex> luaLock(ext.luaMutex);
			sol::protected_function init_p = init;
			sol::protected_function_result result = init_p();
			if (result.valid())
			{
				if (result.return_count() > 0 && result[0].is<int>())
				{
					sleepTime = result[0].as<int>();
				}
			}
			else
			{
				sol::error err = result;
				std::cerr << "init() failed in " << ext.path << ": " << err.what() << "\n";
			}
		}

		auto lastTime = std::chrono::high_resolution_clock::now();
		while (ext.running)
		{
			auto currentTime = std::chrono::high_resolution_clock::now();

			float delta = std::chrono::duration<float, std::milli>(currentTime - lastTime).count();

			lastTime = currentTime;

			try
			{
				// Process pending events on the extension thread safely
				std::vector<std::pair<std::string, CppValue>> eventsToProcess;
				{
					std::lock_guard<std::mutex> lock(ext.eventQueueMutex);
					eventsToProcess = std::move(ext.pendingEvents);
					ext.pendingEvents.clear();
				}

				if (!eventsToProcess.empty())
				{
					std::lock_guard<std::recursive_mutex> luaLock(ext.luaMutex);
					for (const auto &[evtName, cppVal] : eventsToProcess)
					{
						auto it = ext.eventCallbacks.find(evtName);
						if (it != ext.eventCallbacks.end())
						{
							sol::object luaData = cppToLua(ext.lua, cppVal);
							for (auto &[id, cb] : it->second)
							{
								if (cb.valid())
								{
									cb(luaData);
								}
							}
						}
					}
				}

				if (onTick.valid())
				{
					std::lock_guard<std::recursive_mutex> luaLock(ext.luaMutex);
					onTick(delta);
				}
			}
			catch (const std::exception &e)
			{
				std::cerr << "Extension error in " << ext.path << ": " << e.what() << '\n';

				Logger::log("Extension error in " + ext.path.string() + ": " + e.what());
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
				std::lock_guard<std::recursive_mutex> luaLock(ext.luaMutex);
				onExit();
			}
			catch (const std::exception &e)
			{
				std::cerr << "Extension error in onExit for " << ext.path << ": " << e.what() << '\n';
				Logger::log("Extension error in onExit for " + ext.path.string() + ": " + e.what());
			}
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "Extension error in " << ext.path << ": " << e.what() << '\n';
		Logger::log("Extension error in " + ext.path.string() + ": " + e.what());
	}
	catch (...)
	{
		std::cerr << "Unknown extension error in " << ext.path << '\n';
		Logger::log("Unknown extension error in " + ext.path.string());
	}
}

void ExtensionManager::unloadExtension(std::string authorName, std::string extensionName)
{
	for (auto it = extensions.begin(); it != extensions.end();)
	{
		auto &ext = *it;

		if (ext->identity.first == authorName && ext->identity.second == extensionName)
		{
			if (currentWindowManager)
			{
				currentWindowManager->stopTracking();
			}
			unregisterBackendByOwner(authorName, extensionName);
			if (currentWindowManager)
			{
				currentWindowManager->detectAndSetBackend();
				currentWindowManager->startTracking();
			}

			ext->running = false;

			std::thread t = std::move(ext->thread);
			if (t.joinable())
			{
				auto joined = std::make_shared<std::atomic<bool>>(false);
				std::thread joiner(
					[threadToJoin = std::move(t), joined]() mutable
					{
						threadToJoin.join();
						*joined = true;
					});

				int reloadTimeout = AppState::configManager.getConfig("extension-reload-timeout", 450);
				auto start = std::chrono::steady_clock::now();
				while (!*joined)
				{
					if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
							.count() >= reloadTimeout)
					{
						std::cerr << "Extension reload/unload timed out, "
									 "detaching thread "
									 "to prevent hang\n";
						Logger::log("[HPR] Extension reload/unload timed out, "
									"detaching "
									"thread to prevent hang");
						joiner.detach();
						ext->detached = true;
						std::unique_lock<std::recursive_mutex> lock(ext->luaMutex, std::defer_lock);
						if (lock.try_lock())
						{
							ext->lua["HPR"] = sol::nil;
						}
						didTimeoutDuringUnload = true;
						break;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				if (*joined)
				{
					joiner.join();
				}
			}

			{
				std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
				auto &loaded = AppState::state.loadedExtensions;
				loaded.erase(std::remove_if(loaded.begin(), loaded.end(), [&](const auto &e)
											{ return e.first == authorName && e.second == extensionName; }),
							 loaded.end());
				AppState::state.extensionMap.erase({authorName, extensionName});
				it = extensions.erase(it);
			}
		}
		else
		{
			++it;
		}
	}
}

void ExtensionManager::callActionFunction(std::string authorName, std::string extensionName)
{
	std::shared_ptr<LoadedExtension> targetExt = nullptr;
	for (const auto &ext : extensions)
	{
		if (ext->identity.first == authorName && ext->identity.second == extensionName)
		{
			targetExt = ext;
			break;
		}
	}
	if (!targetExt)
	{
		return;
	}

	std::lock_guard<std::recursive_mutex> lock(targetExt->luaMutex);
	sol::protected_function actionFunc = targetExt->lua["onAction"];
	if (!actionFunc.valid())
	{
		actionFunc = targetExt->lua["HPR"]["onAction"];
	}

	if (actionFunc.valid())
	{
		sol::protected_function_result result = actionFunc();
		if (!result.valid())
		{
			sol::error err = result;
			std::cerr << "onAction() failed in extension (" << authorName << "/" << extensionName << "): " << err.what()
					  << "\n";
			Logger::log("[HPR] onAction() failed in extension (" + authorName + "/" + extensionName +
						"): " + err.what() + "\n");
		}
	}
}

void ExtensionManager::reloadMyself(LoadedExtension &ext)
{
	std::string authorName = ext.identity.first;
	std::string extensionName = ext.identity.second;
	std::cout << "[HPR] reloadMyself requested for: " << authorName << " / " << extensionName << std::endl;
	std::thread(
		[this, authorName, extensionName]()
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			reloadExtension(authorName, extensionName);
		})
		.detach();
}

void ExtensionManager::reloadExtension(std::string authorName, std::string extensionName)
{
	std::filesystem::path extPath;
	for (const auto &ext : extensions)
	{
		if (ext->identity.first == authorName && ext->identity.second == extensionName)
		{
			extPath = ext->path;
			break;
		}
	}
	if (!extPath.empty())
	{
		unloadExtension(authorName, extensionName);
	}

	auto newExt = std::make_shared<LoadedExtension>();
	newExt->path = extPath;

	// Pre-initialize Lua state so the script can execute and set config flags
	bool allowNativeLibraries = AppState::configManager.getConfig<bool>("allow-native-libraries", false);
	if (allowNativeLibraries)
	{
		newExt->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math,
								   sol::lib::package);
	}
	else
	{
		newExt->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);
	}
	newExt->lua["HPR"] = newExt->lua.create_table();
	newExt->lua["HPR"]["overrides"] = newExt->lua.create_table();

	try
	{
		newExt->lua.script_file(extPath.string());
	}
	catch (const std::exception &e)
	{
		std::cerr << "Failed to reload extension: " << extPath.string() << "\nError: " << e.what() << '\n';
		Logger::log("[HPR] Failed to reload extension: " + extPath.string() + "\nError: " + e.what() + "\n");
		return;
	}

	sol::optional<bool> useLegacyAPISuffix = newExt->lua["HPR"]["useLegacyAPISuffix"];
	if (useLegacyAPISuffix.has_value())
	{
		newExt->useLegacyAPISuffix = useLegacyAPISuffix.value();
	}

	sol::optional<bool> useHPRTablePrefix = newExt->lua["HPR"]["useHPRTablePrefix"];
	if (useHPRTablePrefix.has_value())
	{
		newExt->useHPRTablePrefix = useHPRTablePrefix.value();
	}

	registerFunctions(*newExt);
	auto ext_copy = newExt;
	newExt->thread = std::thread(&ExtensionManager::runExtension, this, ext_copy);
	extensions.push_back(std::move(newExt));
}

void ExtensionManager::reloadAllExtensions()
{
	if (currentWindowManager)
	{
		currentWindowManager->stopTracking();
	}
	unregisterNonNativeBackends();
	if (currentWindowManager)
	{
		currentWindowManager->detectAndSetBackend();
		currentWindowManager->startTracking();
	}

	std::vector<std::filesystem::path> paths;
	for (const auto &ext : extensions)
		paths.push_back(ext->path);

	// unload all
	for (auto &ext : extensions)
		ext->running = false;
	for (auto &ext : extensions)
	{
		std::thread t = std::move(ext->thread);
		if (t.joinable())
		{
			auto joined = std::make_shared<std::atomic<bool>>(false);
			std::thread joiner(
				[threadToJoin = std::move(t), joined]() mutable
				{
					threadToJoin.join();
					*joined = true;
				});

			int reloadTimeout = AppState::configManager.getConfig("extension-reload-timeout", 450);
			auto start = std::chrono::steady_clock::now();
			while (!*joined)
			{
				if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
						.count() >= reloadTimeout)
				{
					std::cerr << "Extension reload timed out, detaching thread to "
								 "prevent hang\n";
					Logger::log("[HPR] Extension reload timed out, detaching thread to "
								"prevent hang");
					joiner.detach();
					ext->detached = true;
					std::unique_lock<std::recursive_mutex> lock(ext->luaMutex, std::defer_lock);
					if (lock.try_lock())
					{
						ext->lua["HPR"] = sol::nil;
					}
					didTimeoutDuringUnload = true;
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			if (*joined)
			{
				joiner.join();
			}
		}
	}
	extensions.clear();

	// reload all
	for (const auto &path : paths)
	{
		auto newExt = std::make_shared<LoadedExtension>();
		newExt->path = path;

		// Pre-initialize Lua state so the script can execute and set config flags
		bool allowNativeLibraries = AppState::configManager.getConfig<bool>("allow-native-libraries", false);
		if (allowNativeLibraries)
		{
			newExt->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math,
									   sol::lib::package);
		}
		else
		{
			newExt->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);
		}
		newExt->lua["HPR"] = newExt->lua.create_table();
		newExt->lua["HPR"]["overrides"] = newExt->lua.create_table();

		try
		{
			newExt->lua.script_file(path.string());
		}
		catch (const std::exception &e)
		{
			std::cerr << "Failed to reload extension: " << path << "\nError: " << e.what() << '\n';
			Logger::log("[HPR] Failed to reload extension: " + path.string() + "\nError: " + e.what() + "\n");
			continue;
		}

		sol::optional<bool> useLegacyAPISuffix = newExt->lua["HPR"]["useLegacyAPISuffix"];
		if (useLegacyAPISuffix.has_value())
		{
			newExt->useLegacyAPISuffix = useLegacyAPISuffix.value();
		}

		sol::optional<bool> useHPRTablePrefix = newExt->lua["HPR"]["useHPRTablePrefix"];
		if (useHPRTablePrefix.has_value())
		{
			newExt->useHPRTablePrefix = useHPRTablePrefix.value();
		}

		registerFunctions(*newExt);
		extensions.push_back(std::move(newExt));
	}

	// start all threads
	for (auto &ext : extensions)
	{
		auto ext_copy = ext;
		ext->thread = std::thread(&ExtensionManager::runExtension, this, ext_copy);
	}
}

void ExtensionManager::refresh()
{
	try
	{

		// load all lua files from this dir recursively
		for (const auto &entry : std::filesystem::recursive_directory_iterator(extensionPath))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".lua")
			{

				bool alreadyLoaded = false;
				for (const auto &ext : extensions)
				{
					if (std::filesystem::weakly_canonical(ext->path) == std::filesystem::weakly_canonical(entry.path()))
					{
						alreadyLoaded = true;
						break;
					}
				}

				if (alreadyLoaded)
				{
					continue;
				}

				auto ext = std::make_shared<LoadedExtension>();
				ext->path = entry.path();

				// Pre-initialize Lua state so the script can execute and set config flags
				bool allowNativeLibraries = AppState::configManager.getConfig<bool>("allow-native-libraries", false);
				if (allowNativeLibraries)
				{
					ext->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math,
											sol::lib::package);
				}
				else
				{
					ext->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);
				}
				ext->lua["HPR"] = ext->lua.create_table();
				ext->lua["HPR"]["overrides"] = ext->lua.create_table();

				try
				{
					ext->lua.script_file(entry.path().string());
				}
				catch (const std::exception &e)
				{
					std::cerr << "Failed to load extension: " << entry.path() << "\nError: " << e.what() << '\n';
					Logger::log("Failed to load extension: " + entry.path().string() + "\nError: " + e.what());
					continue;
				}

				sol::optional<bool> useLegacyAPISuffix = ext->lua["HPR"]["useLegacyAPISuffix"];
				if (useLegacyAPISuffix.has_value())
				{
					ext->useLegacyAPISuffix = useLegacyAPISuffix.value();
				}

				sol::optional<bool> useHPRTablePrefix = ext->lua["HPR"]["useHPRTablePrefix"];
				if (useHPRTablePrefix.has_value())
				{
					ext->useHPRTablePrefix = useHPRTablePrefix.value();
				}

				registerFunctions(*ext);

				extensions.push_back(std::move(ext));
				auto ext_copy = extensions.back();
				extensions.back()->thread = std::thread(&ExtensionManager::runExtension, this, ext_copy);
			}
			else if (entry.is_regular_file())
			{
				// std::cerr << "Skipping non-lua file: " << entry.path() << '\n';
				// Logger::log("Skipping non-lua file: " + entry.path().string());
			}
		}
	}
	catch (std::exception &e)
	{
		std::cerr << "Error loading extensions: " << e.what() << '\n';
		Logger::log("Error loading extensions: " + std::string(e.what()));
	}
}

void ExtensionManager::registerFunctions(LoadedExtension &ext)
{
	std::string suffix = "";
	// if useLegacyAPISuffix is true, then we will use the legacy API suffix by changing the string to
	//_E
	if (ext.useLegacyAPISuffix)
	{
		suffix = "_E";
		std::cerr << "[HPR] Warning: Extension " << ext.path
				  << " is using the legacy API suffix. This is deprecated but works.\n";
		Logger::log("[HPR] Warning: Extension " + ext.path.string() +
					" is using the legacy API suffix. This is deprecated but works.");
	}
	sol::state &lua = ext.lua;
	// Functions exposed to lua
	bool allowNativeLibraries = AppState::configManager.getConfig<bool>("allow-native-libraries", false);
	if (allowNativeLibraries)
	{
		lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math, sol::lib::package);
	}
	else
	{
		lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);
	}

	if (!lua["HPR"].valid())
	{
		lua["HPR"] = lua.create_table();
	}
	if (!lua["HPR"]["overrides"].valid())
	{
		lua["HPR"]["overrides"] = lua.create_table();
	}

	auto registerFunction = [&](const std::string &name, auto &&callable) {
		lua["HPR"][name] = callable;
		if (!ext.useHPRTablePrefix)
		{
			lua[name] = callable;
		}
	};

	registerFunction("startServer" + suffix, [&ext](int port, sol::function handler) -> bool
	{ return NativeNet::startHttpServer(port, handler, ext); });

	registerFunction("log" + suffix, [](std::string name, std::string msg) { Logger::log("[" + name + "] " + msg); });

	registerFunction("getTime_MS" + suffix, []() -> uint64_t
	{
		if (AppState::extManager)
		{
			auto res = AppState::extManager->dispatchOverride("getTime_MS", {});
			if (res.has_value() && res->type == CppValue::Type::Double)
			{
				return static_cast<uint64_t>(res->double_val);
			}
		}
		return std::chrono::duration_cast<std::chrono::milliseconds>(
				   std::chrono::system_clock::now().time_since_epoch())
			.count();
	});

	auto getExtensionRelativeDir = [this, &ext]() -> std::string
	{
		std::filesystem::path canonicalBase = std::filesystem::weakly_canonical(this->extensionPath);
		std::filesystem::path canonicalExt = std::filesystem::weakly_canonical(ext.path);

		// Security check: ensure the extension path is actually under
		// extensionPath
		auto relative = std::filesystem::relative(canonicalExt.parent_path(), canonicalBase);

		// If it escapes the base directory, return empty string for security
		if (relative.empty() || relative.string().find("..") != std::string::npos)
		{
			return "";
		}

		std::string dir = relative.string();
		if (dir == ".")
		{
			dir = "";
		}

		if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
		{
#ifdef _WIN32
			dir += "\\";
#else
			dir += "/";
#endif
		}
		return dir;
	};

	auto getExtensionAbsoluteDir = [&ext]() -> std::string
	{
		std::string dir = ext.path.parent_path().string();
		if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
		{
#ifdef _WIN32
			dir += "\\";
#else
			dir += "/";
#endif
		}
		return dir;
	};

	registerFunction("getExtensionDir" + suffix, getExtensionRelativeDir);
	registerFunction("getExtensionPath" + suffix, getExtensionRelativeDir);
	registerFunction("getExtensionAbsoluteDir" + suffix, getExtensionAbsoluteDir);

	registerFunction("sleep" + suffix, [&ext](int ms)
	{
		int slept = 0;
		while (slept < ms && ext.running)
		{
			int chunk = (ms - slept < 50) ? (ms - slept) : 50;
			std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
			slept += chunk;
		}
	});

	registerFunction("parseISO8601" + suffix, [](std::string str) -> uint64_t
	{
		int y = 0, m = 0, d = 0, h = 0, min = 0;
		float sec_fraction = 0.0f;
		int parsed = std::sscanf(str.c_str(), "%d-%d-%dT%d:%d:%f", &y, &m, &d, &h, &min, &sec_fraction);
		if (parsed < 5)
			return 0;

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
		if (epoch_sec == -1)
			return 0;

		return static_cast<uint64_t>(epoch_sec) * 1000 + ms;
	});

	registerFunction("httpGet" + suffix, [](std::string host, std::string path, sol::optional<bool> secure,
										sol::optional<sol::table> headers) -> std::tuple<std::string, int>
	{
		std::map<std::string, std::string> cppHeaders;
		if (headers.has_value())
		{
			headers->for_each(
				[&](sol::object k, sol::object v)
				{
					if (k.is<std::string>() && v.is<std::string>())
					{
						cppHeaders[k.as<std::string>()] = v.as<std::string>();
					}
				});
		}
		auto result = NativeNet::httpGet(host, path, secure.value_or(true), cppHeaders);
		return std::make_tuple(result.first, result.second);
	});

	registerFunction("httpPost" + suffix, [](std::string host, std::string path, std::string body,
										 sol::optional<bool> secure,
										 sol::optional<sol::table> headers) -> std::tuple<std::string, int>
	{
		std::map<std::string, std::string> cppHeaders;
		if (headers.has_value())
		{
			headers->for_each(
				[&](sol::object k, sol::object v)
				{
					if (k.is<std::string>() && v.is<std::string>())
					{
						cppHeaders[k.as<std::string>()] = v.as<std::string>();
					}
				});
		}
		auto result = NativeNet::httpPost(host, path, body, secure.value_or(true), cppHeaders);
		return std::make_tuple(result.first, result.second);
	});

	registerFunction("httpPut" + suffix, [](std::string host, std::string path, std::string body,
										sol::optional<bool> secure,
										sol::optional<sol::table> headers) -> std::tuple<std::string, int>
	{
		std::map<std::string, std::string> cppHeaders;
		if (headers.has_value())
		{
			headers->for_each(
				[&](sol::object k, sol::object v)
				{
					if (k.is<std::string>() && v.is<std::string>())
					{
						cppHeaders[k.as<std::string>()] = v.as<std::string>();
					}
				});
		}
		auto result = NativeNet::httpPut(host, path, body, secure.value_or(true), cppHeaders);
		return std::make_tuple(result.first, result.second);
	});

	registerFunction("httpDelete" + suffix, [](std::string host, std::string path, sol::optional<bool> secure,
										   sol::optional<sol::table> headers) -> std::tuple<std::string, int>
	{
		std::map<std::string, std::string> cppHeaders;
		if (headers.has_value())
		{
			headers->for_each(
				[&](sol::object k, sol::object v)
				{
					if (k.is<std::string>() && v.is<std::string>())
					{
						cppHeaders[k.as<std::string>()] = v.as<std::string>();
					}
				});
		}
		auto result = NativeNet::httpDelete(host, path, secure.value_or(true), cppHeaders);
		return std::make_tuple(result.first, result.second);
	});

	registerFunction("parseJSON" + suffix, [&lua](std::string jsonStr, sol::optional<std::string> fieldPath)
	{ return JsonParser::parseJSON_E(lua, jsonStr, fieldPath); });

	registerFunction("toJSON" + suffix, [](sol::object luaTable) { return JsonParser::toJSON_E(luaTable); });

	registerFunction("getCurrentWindow" + suffix, []() { return getCurrentWindow_E(); });

	registerFunction("getCurrentTitle" + suffix, []() { return getCurrentTitle_E(); });

	registerFunction("runSystemCommand" + suffix, [](std::string command) { return runSystemCommand(command); });

	registerFunction("getAlias" + suffix, [](std::string command) { return AppState::aliasManager.getAlias(command); });

	registerFunction("getAlias_Tab" + suffix, [](std::string command)
	{ return AppState::aliasManager.getAlias_Tab(command); });

	registerFunction("getAlias_Project" + suffix, [](std::string command)
	{ return AppState::aliasManager.getAlias_Project(command); });

	registerFunction("getReverseAlias" + suffix, [](std::string aliasName)
	{ return AppState::aliasManager.getReverseAlias(aliasName); });

	registerFunction("getReverseAlias_Tab" + suffix, [](std::string aliasName)
	{ return AppState::aliasManager.getReverseAlias_Tab(aliasName); });

	registerFunction("getReverseAlias_Project" + suffix, [](std::string aliasName)
	{ return AppState::aliasManager.getReverseAlias_Project(aliasName); });

	registerFunction("stopTracking" + suffix, [this]()
	{
		if (AppState::extManager)
		{
			auto res = AppState::extManager->dispatchOverride("stopTracking", {});
			if (res.has_value())
			{
				return;
			}
		}
		currentWindowManager->stopTracking();
	});

	registerFunction("startTracking" + suffix, [this]()
	{
		if (AppState::extManager)
		{
			auto res = AppState::extManager->dispatchOverride("startTracking", {});
			if (res.has_value())
			{
				return;
			}
		}
		currentWindowManager->startTracking();
	});

	registerFunction("registerBackend" + suffix, [&ext, this](std::string name, sol::function matchesEnvironment,
														  sol::function initialize, sol::function getCurrentWindow,
														  sol::function getCurrentTitle, sol::function getCurrentPid)
	{
		auto safeMatches = [&ext, matchesEnvironment](const std::string &env) -> bool
		{
			std::lock_guard<std::recursive_mutex> lock(ext.luaMutex);
			sol::object result = matchesEnvironment(env);
			return result.as<bool>();
		};

		auto safeInitialize = [&ext, initialize]() -> void
		{
			std::lock_guard<std::recursive_mutex> lock(ext.luaMutex);
			initialize();
		};

		auto safeGetCurrentWindow = [&ext, getCurrentWindow]() -> std::string
		{
			std::lock_guard<std::recursive_mutex> lock(ext.luaMutex);
			return getCurrentWindow();
		};

		auto safeGetCurrentTitle = [&ext, getCurrentTitle]() -> std::string
		{
			std::lock_guard<std::recursive_mutex> lock(ext.luaMutex);
			return getCurrentTitle();
		};

		auto safeGetCurrentPid = [&ext, getCurrentPid]() -> std::string
		{
			std::lock_guard<std::recursive_mutex> lock(ext.luaMutex);
			return getCurrentPid();
		};

		registerBackend_E(name, ext.identity.first, ext.identity.second, safeMatches, safeInitialize,
						  safeGetCurrentWindow, safeGetCurrentTitle, safeGetCurrentPid, currentWindowManager);
	});

	registerFunction("dbExecute" + suffix, [this](std::string sql, sol::optional<std::vector<std::string>> params)
	{ dbManager->executeSQL(sql, params.value_or(std::vector<std::string>{})); });

	registerFunction("dbQuery" + suffix, [this](std::string sql, sol::optional<std::vector<std::string>> params)
	{ return dbManager->querySQL(sql, params.value_or(std::vector<std::string>{})); });

	registerFunction("getLoadedHistDbPath" + suffix, [this]() { return dbManager->getLoadedHistDbPath(); });

	registerFunction("dbQueryHistorical" + suffix, [this](std::string sql, sol::optional<std::vector<std::string>> params)
	{
		if (AppState::extManager)
		{
			std::vector<CppValue> cppParams;
			if (params.has_value())
			{
				for (const auto &p : *params)
				{
					cppParams.push_back(CppValue(CppValue::Type::String, p));
				}
			}
			CppValue paramsVal(CppValue::Type::Array);
			paramsVal.array_val = cppParams;

			auto res = AppState::extManager->dispatchOverride("dbQueryHistorical",
															  {CppValue(CppValue::Type::String, sql), paramsVal});
			if (res.has_value())
			{
				std::vector<std::map<std::string, std::string>> results;
				if (res->type == CppValue::Type::Array)
				{
					for (const auto &rowVal : res->array_val)
					{
						if (rowVal.type == CppValue::Type::Struct)
						{
							std::map<std::string, std::string> row;
							for (const auto &[k, v] : rowVal.struct_val)
							{
								if (v.type == CppValue::Type::String)
								{
									row[k] = v.str_val;
								}
								else if (v.type == CppValue::Type::Double)
								{
									row[k] = std::to_string(v.double_val);
								}
								else if (v.type == CppValue::Type::Bool)
								{
									row[k] = v.bool_val ? "true" : "false";
								}
							}
							results.push_back(row);
						}
					}
				}
				return results;
			}
		}

		// Block until the async DB load has finished (or 5s timeout to avoid
		// hanging forever if, e.g., the file wasn't found and load errored out)
		{
			std::unique_lock<std::mutex> lk(AppState::historyLoadedMutex);
			AppState::historyLoadedCV.wait_for(lk, std::chrono::seconds(5),
											   []() { return AppState::historicalData_State.isLoaded; });
		}

		std::string histPath = dbManager->getLoadedHistDbPath();
		if (histPath.empty())
		{
			return std::vector<std::map<std::string, std::string>>{};
		}
		return dbManager->querySQL_Path(histPath, sql, params.value_or(std::vector<std::string>{}));
	});

	registerFunction("getDbPathForDate" + suffix, [](std::string date) -> std::string
	{ return DatabaseManager::getDbPathForDate(date); });

	registerFunction("dbQueryPath" + suffix,
		[this](std::string dbPath, std::string sql, sol::optional<std::vector<std::string>> params)
	{ return dbManager->querySQL_Path(dbPath, sql, params.value_or(std::vector<std::string>{})); });

	registerFunction("dbQueryNumber" + suffix,
		[this,
		 suffix](int days, std::string mode, std::string sql,
				 sol::optional<std::vector<std::string>> params) -> std::vector<std::map<std::string, std::string>>
	{
		auto p = params.value_or(std::vector<std::string>{});
		auto fut = std::async(std::launch::async,
							  [this, days, mode, sql, p]() { return dbManager->dbQueryNumber(days, mode, sql, p); });

		if (fut.wait_for(std::chrono::milliseconds(7500)) == std::future_status::timeout)
		{
			std::cerr << "[HPR] dbQueryNumber" + suffix + " timed out after 7500ms\n";
			Logger::log("[HPR] dbQueryNumber" + suffix + " timed out after 7500ms");
			return {};
		}
		return fut.get().rows;
	});

	registerFunction("dbQueryRange" + suffix,
		[this,
		 suffix](std::string dateFrom, std::string dateTo, std::string mode, std::string sql,
				 sol::optional<std::vector<std::string>> params) -> std::vector<std::map<std::string, std::string>>
	{
		auto p = params.value_or(std::vector<std::string>{});
		auto fut = std::async(std::launch::async, [this, dateFrom, dateTo, mode, sql, p]()
							  { return dbManager->dbQueryRange(dateFrom, dateTo, mode, sql, p); });

		if (fut.wait_for(std::chrono::milliseconds(7500)) == std::future_status::timeout)
		{
			std::cerr << "[HPR] dbQueryRange" + suffix + " timed out after 7500ms\n";
			Logger::log("[HPR] dbQueryRange" + suffix + " timed out after 7500ms");
			return {};
		}
		return fut.get().rows;
	});

	registerFunction("convertToDate_DDMMYY" + suffix, [](uint64_t ms) { return convertToDate_DDMMYY(ms); });

	registerFunction("convertToDate_MMYY" + suffix, [](uint64_t ms) { return convertToDate_MMYY(ms); });

	registerFunction("convertToTime_HHMMSS_12" + suffix, [](uint64_t ms) { return convertToTime_HHMMSS_12(ms); });

	registerFunction("formatTime_HHMMSS" + suffix, [](uint64_t ms) { return formatTime_HHMMSS(ms); });

	registerFunction("parseDate_DDMMYY" + suffix, [](std::string dateStr) { return parseDate_DDMMYY(dateStr); });

	registerFunction("parseDate_MMYY" + suffix, [](std::string dateStr) { return parseDate_MMYY(dateStr); });

	registerFunction("extractMMYY_from_DDMMYY" + suffix, [](std::string dateStr)
	{ return extractMMYY_from_DDMMYY(dateStr); });

	registerFunction("getSystemConfig" + suffix, [](std::string key) -> std::string
	{ return AppState::configManager.getConfig(key, std::string("")); });

	registerFunction("setUiImage" + suffix, [](std::string propertyName, int width, int height, std::string rgbaBytes)
	{
		if (rgbaBytes.size() < static_cast<size_t>(width * height * 4))
			return;

		slint::SharedPixelBuffer<slint::Rgba8Pixel> pixelBuffer(width, height);
		const uint8_t *rawData = reinterpret_cast<const uint8_t *>(rgbaBytes.data());
		slint::Rgba8Pixel *dest = pixelBuffer.begin();
		for (int i = 0; i < width * height; ++i)
		{
			dest[i] = slint::Rgba8Pixel{rawData[i * 4], rawData[i * 4 + 1], rawData[i * 4 + 2], rawData[i * 4 + 3]};
		}

		if (AppState::extManager)
		{
			if (AppState::extManager->app)
			{
				AppState::extManager->app->setUiImage(propertyName, pixelBuffer);
			}
			else if (AppState::extManager->interpreterApp)
			{
				if (UiRegistry::isActive())
				{
					AppState::extManager->interpreterApp->setUiImage(propertyName, pixelBuffer);
				}
			}
		}
	});

	registerFunction("setUiProperty" + suffix, [](std::string name, sol::object value)
	{
		if (AppState::extManager)
		{
			CppValue cppVal = luaToCpp(value);
			auto res = AppState::extManager->dispatchOverride("setUiProperty",
															  {CppValue(CppValue::Type::String, name), cppVal});
			if (res.has_value())
			{
				return;
			}
		}
		if (!UiRegistry::isActive())
		{
			return; // UI is not active/loaded yet
		}

		auto weak = UiRegistry::getInstance();
		// Safely convert Lua object to standard C++ structure on the Lua
		// execution background thread
		CppValue cppVal = luaToCpp(value);

		// Dispatch to Slint's main event loop thread to construct and set the
		// Slint values
		slint::invoke_from_event_loop(
			[weak, name, cppVal]()
			{
				if (auto handle = weak.lock())
				{
					slint::interpreter::Value slintVal = cppToSlint(cppVal);
					(*handle)->set_property(name, slintVal);
				}
			});
	});

	registerFunction("registerUiCallback" + suffix, [&ext](std::string name, sol::function luaCallback)
	{
		if (AppState::extManager)
		{
			auto res =
				AppState::extManager->dispatchOverride("registerUiCallback", {CppValue(CppValue::Type::String, name)});
			if (res.has_value())
			{
				return;
			}
		}
		if (!UiRegistry::isActive())
		{
			return; // no ui
		}

		auto weak = UiRegistry::getInstance();
		slint::invoke_from_event_loop(
			[weak, name, luaCallback, &ext]()
			{
				if (auto handle = weak.lock())
				{
					// lol auto vro iwill be cooked by code reviewers
					(*handle)->set_callback(name,
											[luaCallback, &ext](auto args) -> slint::interpreter::Value
											{
												// Trigger the lua function when Slint fires the
												// callback safely under luaMutex
												std::lock_guard<std::recursive_mutex> lock(ext.luaMutex);

												std::vector<sol::object> luaArgs;
												for (size_t i = 0; i < args.size(); ++i)
												{
													luaArgs.push_back(cppToLua(ext.lua, slintToCpp(args[i])));
												}

												sol::protected_function_result result;
												if (luaArgs.empty())
												{
													result = luaCallback();
												}
												else
												{
													result = luaCallback(sol::as_args(luaArgs));
												}

												if (!result.valid())
												{
													sol::error err = result;
													std::cerr << "[UI CALLBACK ERROR] " << err.what() << std::endl;
													Logger::log("[UI CALLBACK ERROR] " + std::string(err.what()));
												}

												return slint::interpreter::Value(); // void return
											});
				}
			});
	});

	// Connect to system or custom dynamic events
	registerFunction("connect" + suffix, [&lua, &ext](std::string eventName, sol::function callback) -> size_t
	{
		EventKey eventKey;
		if (eventName == "LOAD_DATABASE_SINGULAR")
			eventKey = Event::LOAD_DATABASE_SINGULAR;
		else if (eventName == "HISTORY_LOADED_SINGULAR")
			eventKey = Event::HISTORY_LOADED_SINGULAR;
		else if (eventName == "LOAD_LIVE_DATA")
			eventKey = Event::LOAD_LIVE_DATA;
		else if (eventName == "APP_ERROR")
			eventKey = Event::APP_ERROR;
		else if (eventName == "MIDNIGHT_ROLLOVER")
			eventKey = Event::MIDNIGHT_ROLLOVER;
		else if (eventName == "WINDOW_CHANGED")
			eventKey = Event::WINDOW_CHANGED;
		else if (eventName == "UI_READY")
			eventKey = Event::UI_READY;
		else if (eventName == "LOAD_DATABASE_NUMBER")
			eventKey = Event::LOAD_DATABASE_NUMBER;
		else if (eventName == "LOAD_DATABASE_RANGE")
			eventKey = Event::LOAD_DATABASE_RANGE;
		else if (eventName == "HISTORY_LOADED_NUMBER")
			eventKey = Event::HISTORY_LOADED_NUMBER;
		else if (eventName == "HISTORY_LOADED_RANGE")
			eventKey = Event::HISTORY_LOADED_RANGE;
		else
			eventKey = eventName; // Custom signal

		size_t id = EventHub::connect(eventKey,
									  [eventName, &ext](EventData data)
									  {
										  std::lock_guard<std::mutex> lock(ext.eventQueueMutex);
										  ext.pendingEvents.push_back({eventName, toCppValue(data)});
									  });
		ext.eventCallbacks[eventName][id] = callback;
		ext.registeredConnections.push_back({eventKey, id});
		return id;
	});

	// Unsubscribe from a registered event
	registerFunction("disconnect" + suffix, [&ext](std::string eventName, size_t id)
	{
		EventKey eventKey;
		if (eventName == "LOAD_DATABASE_SINGULAR")
			eventKey = Event::LOAD_DATABASE_SINGULAR;
		else if (eventName == "HISTORY_LOADED_SINGULAR")
			eventKey = Event::HISTORY_LOADED_SINGULAR;
		else if (eventName == "LOAD_LIVE_DATA")
			eventKey = Event::LOAD_LIVE_DATA;
		else if (eventName == "APP_ERROR")
			eventKey = Event::APP_ERROR;
		else if (eventName == "MIDNIGHT_ROLLOVER")
			eventKey = Event::MIDNIGHT_ROLLOVER;
		else if (eventName == "WINDOW_CHANGED")
			eventKey = Event::WINDOW_CHANGED;
		else if (eventName == "UI_READY")
			eventKey = Event::UI_READY;
		else if (eventName == "LOAD_DATABASE_RANGE")
			eventKey = Event::LOAD_DATABASE_RANGE;
		else if (eventName == "HISTORY_LOADED_NUMBER")
			eventKey = Event::HISTORY_LOADED_NUMBER;
		else if (eventName == "HISTORY_LOADED_RANGE")
			eventKey = Event::HISTORY_LOADED_RANGE;

		else
			eventKey = eventName;

		EventHub::disconnect(eventKey, id);
		ext.eventCallbacks[eventName].erase(id);
	});

	// Emit system or custom dynamic events
	registerFunction("emit" + suffix, [](std::string eventName, sol::optional<sol::object> luaData)
	{
		EventKey eventKey;
		if (eventName == "LOAD_DATABASE_SINGULAR")
			eventKey = Event::LOAD_DATABASE_SINGULAR;
		else if (eventName == "HISTORY_LOADED_SINGULAR")
			eventKey = Event::HISTORY_LOADED_SINGULAR;
		else if (eventName == "LOAD_LIVE_DATA")
			eventKey = Event::LOAD_LIVE_DATA;
		else if (eventName == "APP_ERROR")
			eventKey = Event::APP_ERROR;
		else if (eventName == "MIDNIGHT_ROLLOVER")
			eventKey = Event::MIDNIGHT_ROLLOVER;
		else if (eventName == "WINDOW_CHANGED")
			eventKey = Event::WINDOW_CHANGED;
		else if (eventName == "UI_READY")
			eventKey = Event::UI_READY;
		else if (eventName == "LOAD_DATABASE_RANGE")
			eventKey = Event::LOAD_DATABASE_RANGE;
		else if (eventName == "HISTORY_LOADED_NUMBER")
			eventKey = Event::HISTORY_LOADED_NUMBER;
		else if (eventName == "HISTORY_LOADED_RANGE")
			eventKey = Event::HISTORY_LOADED_RANGE;
		else
			eventKey = eventName;

		// Convert Lua parameter to generic CppValue, then map to specific C++
		// EventData structure
		CppValue cppVal = luaData.has_value() ? luaToCpp(luaData.value()) : CppValue();
		EventData data = toEventData(eventKey, cppVal);

		EventHub::emit(eventKey, data);
	});

	auto trim = [](const std::string &str) -> std::string
	{
		size_t first = str.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
			return "";
		size_t last = str.find_last_not_of(" \t\r\n");
		return str.substr(first, last - first + 1);
	};

	auto parseValue = [trim](const std::string &rawValue, sol::state &luaState) -> sol::object
	{
		std::string trimmed = trim(rawValue);
		if (trimmed == "true")
		{
			return sol::make_object(luaState, true);
		}
		if (trimmed == "false")
		{
			return sol::make_object(luaState, false);
		}
		if (!trimmed.empty())
		{
			char *endptr = nullptr;
			double d = std::strtod(trimmed.c_str(), &endptr);
			if (endptr == trimmed.c_str() + trimmed.size())
			{
				return sol::make_object(luaState, d);
			}
		}
		return sol::make_object(luaState, rawValue);
	};

	registerFunction("readCsv" + suffix, [this, &ext, &lua, trim, parseValue](std::string userPath,
																		  sol::optional<sol::object> keyOpt,
																		  sol::this_state ts) -> sol::variadic_results
	{
		sol::variadic_results vr;
		std::string err;
		std::filesystem::path securedPath = resolveAndSecurePath(userPath, this->extensionPath, err);
		if (securedPath.empty())
		{
			std::cerr << "[HPR Extension CSV Error] " << err << " (path: " << userPath << ")" << std::endl;
			Logger::log("[HPR Extension CSV Error] " + err + " (path: " + userPath + ")");
			vr.push_back(sol::make_object(ts, ""));
			return vr;
		}

		std::ifstream file(securedPath);
		if (!file.is_open())
		{
			std::cerr << "[HPR Extension CSV Error] Failed to open file for reading: " << securedPath << std::endl;
			Logger::log("[HPR Extension CSV Error] Failed to open file for reading: " + securedPath.string());
			vr.push_back(sol::make_object(ts, ""));
			return vr;
		}

		std::vector<std::string> keys;
		std::vector<std::string> values;
		std::string line;
		while (std::getline(file, line))
		{
			if (!line.empty() && line.back() == '\n')
				line.pop_back();
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.empty())
				continue;

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
			if (kObj.is<std::string>())
				targetKey = kObj.as<std::string>();
			else if (kObj.is<double>())
			{
				std::ostringstream oss;
				oss << kObj.as<double>();
				targetKey = oss.str();
			}
			else if (kObj.is<bool>())
				targetKey = kObj.as<bool>() ? "true" : "false";

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
	});

	registerFunction("writeCsv" + suffix, [this, &ext, &lua](std::string userPath, sol::object key,
														 sol::object value) -> bool
	{
		std::string err;
		std::filesystem::path securedPath = resolveAndSecurePath(userPath, this->extensionPath, err);
		if (securedPath.empty())
		{
			std::cerr << "[HPR Extension CSV Error] " << err << " (path: " << userPath << ")" << std::endl;
			Logger::log("[HPR Extension CSV Error] " + err + " (path: " + userPath + ")");
			return false;
		}

		std::string keyStr;
		if (key.is<std::string>())
			keyStr = key.as<std::string>();
		else if (key.is<double>())
		{
			std::ostringstream oss;
			oss << key.as<double>();
			keyStr = oss.str();
		}
		else if (key.is<bool>())
			keyStr = key.as<bool>() ? "true" : "false";
		else
			return false;

		std::string valStr;
		if (value.is<std::string>())
			valStr = value.as<std::string>();
		else if (value.is<double>())
		{
			std::ostringstream oss;
			oss << value.as<double>();
			valStr = oss.str();
		}
		else if (value.is<bool>())
			valStr = value.as<bool>() ? "true" : "false";
		else
			return false;

		try
		{
			std::filesystem::create_directories(securedPath.parent_path());
		}
		catch (const std::exception &e)
		{
			std::cerr << "[HPR Extension CSV Error] Failed to create "
						 "directories for: "
					  << securedPath << " (" << e.what() << ")" << std::endl;
			Logger::log("[HPR Extension CSV Error] Failed to create directories for: " + securedPath.string() + " (" +
						e.what() + ")");
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
				if (!line.empty() && line.back() == '\n')
					line.pop_back();
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				if (line.empty())
					continue;

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

		for (auto &entry : entries)
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
			Logger::log("[HPR Extension CSV Error] Failed to open file for writing: " + securedPath.string());
			return false;
		}

		for (const auto &entry : entries)
		{
			outFile << entry.first << "," << entry.second << "\n";
		}
		outFile.close();

		return true;
	});

	registerFunction("deleteCsv" + suffix, [this, &ext](std::string userPath) -> bool
	{
		std::string err;
		std::filesystem::path securedPath = resolveAndSecurePath(userPath, this->extensionPath, err);
		if (securedPath.empty())
		{
			std::cerr << "[HPR Extension CSV Error] " << err << " (path: " << userPath << ")" << std::endl;
			Logger::log("[HPR Extension CSV Error] " + err + " (path: " + userPath + ")");
			return false;
		}

		try
		{
			if (std::filesystem::exists(securedPath))
			{
				return std::filesystem::remove(securedPath);
			}
			return false;
		}
		catch (const std::exception &e)
		{
			std::cerr << "[HPR Extension CSV Error] Failed to delete file: " << securedPath << " (" << e.what() << ")"
					  << std::endl;
			Logger::log("[HPR Extension CSV Error] Failed to delete file: " + securedPath.string() + " (" + e.what() +
						")");
			return false;
		}
	});

	registerFunction("generateInsights" + suffix, []()
	{
		std::lock_guard<std::mutex> lock(AppState::patternAnalyzerMutex);
		AppState::patternAnalyzer.generateInsights();
	});

	registerFunction("getMostUsed" + suffix, []() -> std::string
	{
		std::lock_guard<std::mutex> lock(AppState::patternAnalyzerMutex);
		return AppState::patternAnalyzer.getMostUsed();
	});

	registerFunction("getTotalTrackedTime" + suffix, []() -> std::string
	{
		std::lock_guard<std::mutex> lock(AppState::patternAnalyzerMutex);
		return AppState::patternAnalyzer.getTotalTrackedTime();
	});

	registerFunction("getSwitchCount" + suffix, []() -> std::string
	{
		std::lock_guard<std::mutex> lock(AppState::patternAnalyzerMutex);
		return AppState::patternAnalyzer.getSwitchCount();
	});

	registerFunction("getMostSwitchedFrom" + suffix, []() -> std::string
	{
		std::lock_guard<std::mutex> lock(AppState::patternAnalyzerMutex);
		return AppState::patternAnalyzer.getMostSwitchedFrom();
	});

	registerFunction("getMostSwitchedTo" + suffix, []() -> std::string
	{
		std::lock_guard<std::mutex> lock(AppState::patternAnalyzerMutex);
		return AppState::patternAnalyzer.getMostSwitchedTo();
	});

	registerFunction("getMostFocusedSession" + suffix, []() -> std::string
	{
		std::lock_guard<std::mutex> lock(AppState::patternAnalyzerMutex);
		return AppState::patternAnalyzer.getMostFocusedSession();
	});

	registerFunction("getMostProductiveHour" + suffix, []() -> std::string
	{
		std::lock_guard<std::mutex> lock(AppState::patternAnalyzerMutex);
		return AppState::patternAnalyzer.getMostProductiveHour();
	});

	registerFunction("showUi" + suffix, [this]()
	{
		if (app)
			app->show();
		else if (interpreterApp)
			interpreterApp->show();
	});

	registerFunction("hideUi" + suffix, [this]()
	{
		if (app)
			app->hide();
		else if (interpreterApp)
			interpreterApp->hide();
	});

	registerFunction("quitUi" + suffix, [this]()
	{
		if (app)
			app->quit();
		else if (interpreterApp)
			interpreterApp->quit();
	});

	registerFunction("showUiPopup" + suffix,
		[this, &ext](std::string text, std::string leftText, std::string rightText, sol::function luaCallback)
	{
		this->showUiPopup(text, leftText, rightText, true,
						  [luaCallback, &ext](int btn)
						  {
							  std::lock_guard<std::recursive_mutex> lock(ext.luaMutex);
							  if (luaCallback.valid())
							  {
								  auto res = luaCallback(btn);
								  if (!res.valid())
								  {
									  sol::error err = res;
									  std::cerr << "showUiPopup callback failed: " << err.what() << "\n";
								  }
							  }
						  });
	});

	registerFunction("showNotification" + suffix, [](std::string title, std::string message)
	{
		Logger::log("[HPR Extension Notification] " + title + ": " + message);
		showNotification(title, message);
	});

	registerFunction("getOsName" + suffix, []() -> std::string
	{
#ifdef _WIN32
		return "Windows";
#elif defined(__APPLE__)
		return "Apple";
#else
		return "Linux";
#endif
	});

	registerFunction("getEnvironmentName" + suffix, []() -> std::string
	{
#ifdef _WIN32
		return "";
#else
		std::string cmd = "echo $XDG_CURRENT_DESKTOP";
		std::string result = runSystemCommand(cmd);
		result.erase(result.find_last_not_of(" \n\r\t") + 1);
		return result;
#endif
	});

	registerFunction("getLoadedExtensions" + suffix, [&lua]() -> sol::table
	{
		std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
		sol::table listTable = lua.create_table();
		int index = 1;
		for (const auto &identity : AppState::state.loadedExtensions)
		{
			sol::table entryTable = lua.create_table();
			entryTable["authorName"] = identity.first;
			entryTable["extensionName"] = identity.second;
			listTable[index++] = entryTable;
		}
		return listTable;
	});

	registerFunction("crash" + suffix, [suffix](sol::optional<std::string> message)
	{
		if (message.has_value())
		{
			Logger::log("[HPR Extension Crash] " + message.value());
			std::cerr << "[HPR Extension Crash] " << message.value() << std::endl;
		}
		else
		{
			Logger::log("[HPR Extension Crash] HPR has been intentionally crashed "
						"via HPR.crash" +
						suffix + "() by an extension!");
			std::cerr << "[HPR Extension Crash] HPR has been intentionally crashed "
						 "via HPR.crash" +
							 suffix + "() by an extension!"
					  << std::endl;
		}
#ifdef _WIN32
		TerminateProcess(GetCurrentProcess(), 0);
#else
		_exit(0);
#endif
	});

	registerFunction("getLiveTimeLogPerApp" + suffix, [&lua]() -> sol::table
	{
		std::map<std::string, uint64_t> copy = getLiveTimeLogPerApp_E();
		sol::table result = lua.create_table();
		for (const auto &[app, ms] : copy)
		{
			result[app] = ms;
		}
		return result;
	});

	registerFunction("getLiveTimeLogPerTab" + suffix, [&lua]() -> sol::table
	{
		std::map<std::string, uint64_t> copy = getLiveTimeLogPerTab_E();
		sol::table result = lua.create_table();
		for (const auto &[tab, ms] : copy)
		{
			result[tab] = ms;
		}
		return result;
	});

	registerFunction("getLiveTimeLogPerProject" + suffix, [&lua]() -> sol::table
	{
		std::map<std::string, uint64_t> copy = getLiveTimeLogPerProject_E();
		sol::table result = lua.create_table();
		for (const auto &[project, ms] : copy)
		{
			result[project] = ms;
		}
		return result;
	});

	registerFunction("getPid" + suffix, [](std::string rawName) -> std::string
	{
		std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
		return AppState::state.appNamePid[rawName];
	});

	registerFunction("killPid" + suffix, [](std::string pid)
	{
#ifdef _WIN32
		std::string cmd = "taskkill /F /PID " + pid;
		runSystemCommand_UNSAFE(cmd);
#else
		std::string cmd = "kill -9 " + pid + " 2>/dev/null";
		runSystemCommand_UNSAFE(cmd);
#endif
	});

	registerFunction("setLimit" + suffix, [](std::string name, int minutes)
	{
		std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
		if (AppState::limitsManager)
		{
			AppState::limitsManager->setLimit(name, minutes);
		}
	});

	registerFunction("getLimit" + suffix, [](std::string name) -> int
	{
		std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
		if (AppState::state.appLimits.find(name) == AppState::state.appLimits.end())
		{
			return -1; // No limit set
		}
		return AppState::state.appLimits[name];
	});

	registerFunction("setGoal" + suffix, [](std::string name, int minutes)
	{
		std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
		if (AppState::limitsManager)
		{
			AppState::limitsManager->setGoal(name, minutes);
		}
	});

	registerFunction("getGoal" + suffix, [](std::string name) -> int
	{
		std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
		if (AppState::state.appGoals.find(name) == AppState::state.appGoals.end())
		{
			return -1; // No goal set
		}
		return AppState::state.appGoals[name];
	});

	registerFunction("unloadExtension" + suffix, [this](std::string authorName, std::string extensionName)
	{ unloadExtension(authorName, extensionName); });

	registerFunction("reloadMyself" + suffix, [this, &ext]() { reloadMyself(ext); });

	registerFunction("reloadExtension" + suffix, [this](std::string authorName, std::string extensionName)
	{ reloadExtension(authorName, extensionName); });

	registerFunction("refreshExtensions" + suffix, [this]() { refresh(); });

	registerFunction("applyTheme" + suffix, [this](std::string themeName)
	{
		std::string path;
		path = AppState::themeManager.getPathByName(themeName);

		if (!path.empty())
		{
			if (interpreterApp)
			{
				interpreterApp->reload(path);
				AppState::configManager.setConfig("custom-theme", themeName);
			}
		}
		else if (themeName == "Default" || themeName == "default")
		{
			if (interpreterApp)
			{
				interpreterApp->reload("");
			}
			AppState::configManager.setConfig("custom-theme", std::string("Default"));
		}
	});

	registerFunction("getThemeNames" + suffix, [this, &lua]() -> sol::table
	{
		auto themeNames = AppState::themeManager.getThemeNames();

		sol::table listTable = lua.create_table();
		int index = 1;
		for (const auto &name : themeNames)
			listTable[index++] = name;

		return listTable;
	});

	registerFunction("getCurrentThemeName" + suffix, [this]() -> std::string
	{ return AppState::themeManager.getCurrentThemeName(); });

	registerFunction("amICompatible" + suffix, [this, &ext]() -> bool { return ext.isCompatible; });

	registerFunction("getHPRVersion" + suffix, [this]() -> std::string
	{
		return AppState::APP_VERSION;
	});
}

void ExtensionManager::updateExtensionPath()
{
#ifdef _WIN32
	extensionPath = std::getenv("APPDATA");
	extensionPath /= std::filesystem::path("HPR/HPR_Config/extensions/");
#else
	const char *home = std::getenv("HOME");
	if (!home)
		throw std::runtime_error("HOME env var not set");
	extensionPath = home;
	extensionPath /= std::filesystem::path(".config/HPR/extensions/");
#endif

	std::filesystem::create_directories(extensionPath);
}

std::filesystem::path resolveAndSecurePath(const std::string &userPath, const std::filesystem::path &baseDir,
										   std::string &errOut)
{
	std::filesystem::path userP(userPath);
	if (userP.is_absolute() || userPath.rfind("/", 0) == 0 || userPath.rfind("\\", 0) == 0 ||
		(userPath.size() >= 2 && userPath[1] == ':'))
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

std::optional<CppValue> ExtensionManager::dispatchOverride(const std::string &overrideName,
														   const std::vector<CppValue> &args)
{
	std::vector<std::shared_ptr<LoadedExtension>> extCopy;
	{
		std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
		extCopy = extensions;
	}

	for (auto &ext : extCopy)
	{
		if (!ext || !ext->running)
			continue;

		std::unique_lock<std::recursive_mutex> luaLock(ext->luaMutex, std::defer_lock);
		if (!luaLock.try_lock())
		{
			continue;
		}

		sol::table HPR = ext->lua["HPR"];
		if (!HPR.valid())
			continue;

		sol::object overridesObj = HPR["overrides"];
		if (!overridesObj.valid() || !overridesObj.is<sol::table>())
			continue;

		sol::table overrides = overridesObj.as<sol::table>();

		sol::optional<sol::protected_function> overrideFunc = overrides[overrideName];
		if (overrideFunc.has_value() && overrideFunc.value().valid())
		{
			try
			{
				std::vector<sol::object> luaArgs;
				for (const auto &arg : args)
				{
					luaArgs.push_back(cppToLua(ext->lua, arg));
				}

				sol::protected_function_result result = overrideFunc.value()(sol::as_args(luaArgs));

				if (result.valid())
				{
					if (result.return_count() == 0)
						continue;

					sol::object returnedObject = result.get<sol::object>(0); // <-- fix here

					if (!returnedObject.valid() || returnedObject.is<sol::nil_t>())
						continue;

					return luaToCpp(returnedObject);
				}
				else
				{
					sol::error err = result;
					std::cerr << "[HPR Override Error] Lua error in override '" << overrideName << "': " << err.what()
							  << std::endl;
				}
			}
			catch (const std::exception &e)
			{
				std::cerr << "[HPR Override Error] Error executing override '" << overrideName << "' in extension '"
						  << ext->identity.second << "': " << e.what() << std::endl;
				Logger::log("[HPR Override Error] " + std::string(e.what()));
			}
		}
	}
	return std::nullopt;
}

void ExtensionManager::showUiPopup(const std::string &text, const std::string &leftBtnText,
								   const std::string &rightBtnText, bool isLua, std::function<void(int)> callback)
{
	std::lock_guard<std::mutex> lock(queueMutex);
	uint64_t reqId = nextPopupId++;
	popupQueue.push({reqId, text, leftBtnText, rightBtnText, isLua, callback});
	if (!isPopupActive)
	{
		showNextPopup_Unlocked();
	}
}

void ExtensionManager::showNextPopup_Unlocked()
{
	if (popupQueue.empty())
	{
		return;
	}

	isPopupActive = true;
	PopupRequest req = popupQueue.front();
	popupQueue.pop();
	currentPopupCallback = req.callback;
	activePopupId = req.id;

	slint::invoke_from_event_loop(
		[weak_compiled = compiledUiWeak, weak_interpreted = interpretedUiWeak, text = req.text, left = req.leftBtnText,
		 right = req.rightBtnText]()
		{
			if (auto handle = weak_compiled.lock())
			{
				(*handle)->set_uiPopupText_S(slint::SharedString(text));
				(*handle)->set_uiPopupLeftBtnText_S(slint::SharedString(left));
				(*handle)->set_uiPopupRightBtnText_S(slint::SharedString(right));
				(*handle)->set_showUiPopup_S(true);
			}
			else if (auto handle_int = weak_interpreted.lock())
			{
				(*handle_int)->set_property("uiPopupText_S", slint::interpreter::Value(slint::SharedString(text)));
				(*handle_int)
					->set_property("uiPopupLeftBtnText_S", slint::interpreter::Value(slint::SharedString(left)));
				(*handle_int)
					->set_property("uiPopupRightBtnText_S", slint::interpreter::Value(slint::SharedString(right)));
				(*handle_int)->set_property("showUiPopup_S", slint::interpreter::Value(true));
			}
		});

	if (req.isLua)
	{
		std::thread(
			[this, req_id = req.id]()
			{
				int timeoutMs = AppState::configManager.getConfig<int>("extension-popup-timeout", 7500);
				std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));

				std::lock_guard<std::mutex> lock(this->queueMutex);
				if (this->isPopupActive && this->activePopupId == req_id)
				{
					slint::invoke_from_event_loop(
						[weak_compiled = this->compiledUiWeak, weak_interpreted = this->interpretedUiWeak, this]()
						{
							if (auto handle = weak_compiled.lock())
							{
								(*handle)->set_showUiPopup_S(false);

								if (this->currentPopupCallback)
								{
									auto cb = this->currentPopupCallback;
									this->currentPopupCallback = nullptr;
									cb(-1);
								}

								std::lock_guard<std::mutex> lock2(this->queueMutex);
								if (!this->popupQueue.empty())
								{
									this->showNextPopup_Unlocked();
								}
								else
								{
									this->isPopupActive = false;
								}
							}
							else if (auto handle_int = weak_interpreted.lock())
							{
								(*handle_int)->set_property("showUiPopup_S", slint::interpreter::Value(false));

								if (this->currentPopupCallback)
								{
									auto cb = this->currentPopupCallback;
									this->currentPopupCallback = nullptr;
									cb(-1);
								}

								std::lock_guard<std::mutex> lock2(this->queueMutex);
								if (!this->popupQueue.empty())
								{
									this->showNextPopup_Unlocked();
								}
								else
								{
									this->isPopupActive = false;
								}
							}
						});
				}
			})
			.detach();
	}
}