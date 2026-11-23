#include "extensionManager.hpp"

#include "appEvents.hpp"
#include "appState.hpp"
#include "currentWindowManager.hpp"
#include "extensionManager.hpp"
#include "netUtilities.hpp"
#include "app-window.h"
#include "windowBackendRegistery.hpp"
#include "logger.hpp"

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

void ExtensionManager::runExtension(std::shared_ptr<LoadedExtension> extPtr)
{
	auto &ext = *extPtr;
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

		checkCompatibility(ext);

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

		// Special case for official HPR-Store
		// used to fix the dynamic lib versioning hell
		// If we found HPR store but it doesnt support current version of HPR, we will skip loading it and log a warning to the user
		if (resolvedName == "HPR Store" && resolvedAuthor == "Plexescor")
		{
			bool foundMatchingVersion = false;
			for (std::string& version : versionsVec)
			{
				// Special case for this versino because in this version slint was upgraded for HPR
				// so you may need to manually update the store
				// this bad code itches me
				if (version == "0.9.13")
				{
					foundMatchingVersion = true;
					break;
				}
			}
			if (!foundMatchingVersion)
			{
				std::cerr << "HPR-Store extension version mismatch, skipping load, you may need to manually update HPR-Store\n";
				Logger::log("HPR-Store extension version mismatch, skipping load, you may need to manually update HPR-Store");
				ext.running = false;
				{
					std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
					AppState::state.loadedExtensions.erase(
						std::remove(AppState::state.loadedExtensions.begin(),
									AppState::state.loadedExtensions.end(), ext.identity),
						AppState::state.loadedExtensions.end());
					AppState::state.extensionMap.erase(ext.identity);
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 

				std::string text =
					"HPR-Store: UPDATE NEEDED\n"
					"This version of HPR doesn't support the HPR-Store's currently installed version at all.\n"
					"This is because both were built with different versions of slint.\n"
					"Would you like for HPR to auto-update it?";
			
				this->showUiPopup(
					text,
					"No",
					"Yes",
					false,
					[this, extPtr](int button)
					{
						if (button == 1)
						{
							std::cerr << "User chose to update HPR-Store\n";
							Logger::log("User chose to update HPR-Store");
							auto upd = updateManager ? updateManager : AppState::updateManager;
							if (upd)
							{
								std::filesystem::path storePath = extPtr.get()->path.parent_path();
								Logger::log("Updating HPR-Store at path: " + storePath.string());
								upd->updateHPRStore(storePath);
							}
						}
						else
						{
							std::cerr << "User chose not to update HPR-Store\n";
							Logger::log("User chose not to update HPR-Store");
						}
					}
				);
					
				return;
			}
		}
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
							sol::object luaData = LuaCPPSlint::cppToLua(ext.lua, cppVal);
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
