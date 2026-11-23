#include <atomic>
#include <filesystem>
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
#include <slint-interpreter.h>

#include "sol.hpp"
#include "appEvents.hpp"
#include "appState.hpp"
#include "currentWindowManager.hpp"
#include "extensionManager.hpp"
#include "netUtilities.hpp"
#include "app-window.h"
#include "windowBackendRegistery.hpp"

extern "C" HPR_API void HPR_invokeOnSlintThread(void(*fn)(void*), void* userdata)
{
    slint::invoke_from_event_loop([fn, userdata]()
    {
        fn(userdata);
    });
}


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

	// check if config.csv defines an extensions path
	// if yes then switch to it, else keep the platform default above
	std::filesystem::path customPath = std::filesystem::path(
			AppState::configManager.getConfig<std::string>("extensions-root-folder-path", "")
		);

	if (!customPath.empty())
	{
		if (customPath.is_absolute())
		{
			extensionPath = customPath;
			Logger::log("[ExtensionManager] Using extensions path from config.csv: " + extensionPath.string());
		}
		else
		{
			std::cerr << "[ExtensionManager] The custom provided extensions path is not absolute: "
					  << customPath.string() << std::endl;
			Logger::log(std::string("[ExtensionManager] The custom provided extensions path is not absolute: ") +
						customPath.string());
		}
	}

	// create_directories already-existing dirs are fine (no-op); but permission or invalid-path
	// failures would throw and abort startup from the constructor, so report and continue instead
	std::error_code fsError;
	std::filesystem::create_directories(extensionPath, fsError);
	if (fsError)
	{
		std::cerr << "[ExtensionManager] Failed to create extensions directory: " << fsError.message() << " ("
				  << extensionPath.string() << ")" << std::endl;
		Logger::log("[ExtensionManager] Failed to create extensions directory: " + fsError.message() + " (" +
					extensionPath.string() + ")");
	}
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

		sol::object hprObj = ext->lua["HPR"];
		if (!hprObj.valid() || !hprObj.is<sol::table>())
			continue;
		sol::table HPR = hprObj.as<sol::table>();

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
					luaArgs.push_back(LuaCPPSlint::cppToLua(ext->lua, arg));
				}

				sol::protected_function_result result = overrideFunc.value()(sol::as_args(luaArgs));

				if (result.valid())
				{
					if (result.return_count() == 0)
						continue;

					sol::object returnedObject = result.get<sol::object>(0); // <-- fix here

					if (!returnedObject.valid() || returnedObject.is<sol::nil_t>())
						continue;

					return LuaCPPSlint::luaToCpp(returnedObject);
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

void ExtensionManager::checkCompatibility(LoadedExtension &ext)
{
	std::string currentAppVer = AppState::APP_VERSION;
	if (!currentAppVer.empty() && (currentAppVer[0] == 'v' || currentAppVer[0] == 'V'))
	{
		currentAppVer = currentAppVer.substr(1);
	}

	if (!ext.hasVersionSupport)
	{
		ext.isCompatible = false;
		ext.warningMessage = "Unknown version compatibility for HPR " + currentAppVer;
	}
	else
	{
		bool matchFound = false;
		for (const auto &v : ext.versionSupport)
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
			for (size_t i = 0; i < ext.versionSupport.size(); ++i)
			{
				if (i > 0)
					supportedStr += ", ";
				supportedStr += ext.versionSupport[i];
			}
			ext.warningMessage = "This extension is incompatible with HPR " + currentAppVer +
								 ", the versions supported by the extension are: (" + supportedStr + ")";
		}
	}
}