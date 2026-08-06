#pragma once
#include "currentWindowManager.hpp"
#include "databaseManager.hpp"
#include "linuxUtilities.hpp"
#include "sol.hpp"
#include "trayManager.hpp"
#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <slint.h>
#include <slint-interpreter.h>

#include "appEvents.hpp"

// FORWARD DECLARE TO AVOID CIRCULAR DEPENDENCIES
class HPR;
class HPRInterpreter;
class MainWindow;
namespace slint::interpreter {
class ComponentInstance;
}

struct PopupRequest
{
	uint64_t id;
	std::string text;
	std::string leftBtnText;
	std::string rightBtnText;
	bool isLua;
	std::function<void(int)> callback;
};

struct LoadedExtension
{
	std::filesystem::path path;
	sol::state lua;
	std::thread thread;
	std::atomic<bool> running{true};
	std::atomic<bool> detached{false};
	std::pair<std::string, std::string> identity;
	std::recursive_mutex luaMutex; // Guards all shared Lua VM operations recursively to prevent
								   // multi-threaded race conditions without deadlocking
	std::recursive_mutex serverMutex;
	std::vector<std::pair<EventKey, size_t>> registeredConnections; // Tracks registered EventHub connections to
																	// clean up on destruction

	std::map<std::string, std::map<size_t, sol::function>> eventCallbacks;
	std::mutex eventQueueMutex;
	std::vector<std::pair<std::string, CppValue>> pendingEvents;

	bool useLegacyAPISuffix = false;

	std::vector<std::string> versionSupport;
	bool hasVersionSupport = false;
	bool isCompatible = true;
	std::string warningMessage;

	LoadedExtension() = default;
	~LoadedExtension();
	LoadedExtension(const LoadedExtension &) = delete;
	LoadedExtension &operator=(const LoadedExtension &) = delete;
};

class ExtensionManager
{
  public:
	ExtensionManager();
	~ExtensionManager();
	void run();
	void loadExtensions();

	void reloadExtension(std::string authorName, std::string extensionName);
	void unloadExtension(std::string authorName, std::string extensionName);
	void reloadAllExtensions();
	void refresh();

	std::optional<CppValue> dispatchOverride(const std::string &overrideName, const std::vector<CppValue> &args);

  private:
	void updateExtensionPath();
	void registerFunctions(LoadedExtension &ext);
	void runExtension(std::shared_ptr<LoadedExtension> ext);

  private:
	std::vector<std::shared_ptr<LoadedExtension>> extensions;
	std::filesystem::path extensionPath;

	bool didTimeoutDuringUnload = false;

  public:
	// Slint handles
	slint::ComponentWeakHandle<MainWindow> compiledUiWeak;
	slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> interpretedUiWeak;

	// Popup queue & state
	std::queue<PopupRequest> popupQueue;
	std::function<void(int)> currentPopupCallback = nullptr;
	bool isPopupActive = false;
	std::mutex queueMutex;
	uint64_t nextPopupId = 1;
	uint64_t activePopupId = 0;

	void showUiPopup(const std::string &text, const std::string &leftBtnText, const std::string &rightBtnText, bool isLua, std::function<void(int)> callback);
	void showNextPopup_Unlocked();

	// some shit
	DatabaseManager *dbManager = nullptr;
	TrayManager *trayManager = nullptr;
	CurrentWindowManager *currentWindowManager = nullptr;
	HPR *app = nullptr;
	HPRInterpreter *interpreterApp = nullptr;
	LinuxInitialiser *linuxInit = nullptr;
};

std::filesystem::path resolveAndSecurePath(const std::string &userPath, const std::filesystem::path &baseDir,
										   std::string &errOut);