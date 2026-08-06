#include "HPRInterpreter.hpp"

#include <slint-interpreter.h>

#include "aliasManager.hpp"
#include "appEvents.hpp"
#include "appState.hpp"
#include "currentWindowManager.hpp"
#include "extensionManager.hpp"
#include "logger.hpp"
#include "patternAnalyzer.hpp"
#include "timeUtils.hpp"
#include "uiEventBridge.hpp"
#include "uiRegistry.hpp"
#include "windowUtilities.hpp"
#include "telemetryManager.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include "Windows.h"
#endif

HPRInterpreter::HPRInterpreter(ExtensionManager *extMgr)
{
	if (extMgr)
		this->extManager = extMgr;

	if (!initialiseSlintUiPath())
		exit(1);

	std::string savedTheme = AppState::configManager.getConfig("custom-theme", std::string("Default"));
	std::string pathToLoad = (filePath / fileName).string();
	if (savedTheme != "Default" && savedTheme != "default" && AppState::themeManager.availableThemes_Bare.contains(savedTheme))
	{
		pathToLoad = AppState::themeManager.availableThemes_Bare[savedTheme];
	}

	compiler = std::make_unique<slint::interpreter::ComponentCompiler>();
	definition = compiler->build_from_path(pathToLoad);

	if (!definition.has_value())
	{
		for (auto &diag : compiler->diagnostics())
		{
			fprintf(stderr, "  → %s\n", diag.message.data());
			Logger::log(diag.message.data());
		}
		exit(1);
	}

	instance = definition->create();
	weak_instance = instance.value();

	// give ui registery for what it demands
	UiRegistry::registerInstance(weak_instance.value());

	modelManager.emplace(instance.value());
#ifdef __linux__
	slint::set_xdg_app_id("HPR"); // So it has a class in hyprland
#endif
}

void HPRInterpreter::saveWindowGeometry()
{
	if (!instance.has_value())
		return;

	auto weak = slint::ComponentWeakHandle<slint::interpreter::ComponentInstance>(instance.value());
	slint::invoke_from_event_loop(
		[weak]()
		{
			if (auto handle = weak.lock())
			{
				static int lastWidth = -1;
				static int lastHeight = -1;
				static int lastX = -1;
				static int lastY = -1;

				auto size = (*handle)->window().size();
				auto pos = (*handle)->window().position();

				int w = static_cast<int>(size.width);
				int h = static_cast<int>(size.height);
				int x = pos.x;
				int y = pos.y;

				if (w > 200 && h > 200)
				{
					if (w != lastWidth || h != lastHeight || x != lastX || y != lastY)
					{
						AppState::configManager.setConfig("window-width", w);
						AppState::configManager.setConfig("window-height", h);
						AppState::configManager.setConfig("window-pos-x", x);
						AppState::configManager.setConfig("window-pos-y", y);
						lastWidth = w;
						lastHeight = h;
						lastX = x;
						lastY = y;
					}
				}
			}
		});
}

HPRInterpreter::~HPRInterpreter()
{
	running = false;

	// Wake up the thread if it's trapped in a hidden pause cycle during
	// shutdown
	pauseCv.notify_all();

	if (tracker.joinable())
		tracker.join(); // Speaks for itself

	EventHub::disconnect(Event::APP_ERROR, errorId);
}

void HPRInterpreter::reload(std::string path)
{
	slint::invoke_from_event_loop(
		[this, path]()
		{
			// local compiler, no need to store it — just swap definition + instance
			slint::interpreter::ComponentCompiler newCompiler;

			std::optional<slint::interpreter::ComponentDefinition> newDef;

			if (path == "")
				newDef = newCompiler.build_from_path((filePath / fileName).string());
			else
				newDef = newCompiler.build_from_path(path);

			if (!newDef.has_value())
			{
				EventHub::emit(Event::APP_ERROR, ErrorGui{"Hot reload failed: compile error"});
				for (auto &diag : newCompiler.diagnostics())
					fprintf(stderr, "[HotReload] → %s\n", diag.message.data());
				return;
			}

			// ComponentHandle is NOT optional, create() returns it directly
			auto newInst = newDef->create();

			// grab old geometry before touching anything
			auto oldPos = instance.value()->window().position();

			// lock so trackingLoop doesn't touch modelManager mid-swap
			{
				std::lock_guard<std::mutex> lock(reloadMutex);
				UiRegistry::registerInstance(newInst);
				modelManager.emplace(newInst);
			}

			// re-wire UI event callbacks on new instance
			uiEventBridge.emplace(newInst, extManager, this);

			// show new BEFORE hiding oldanti-flicker
			newInst->show();
			instance.value()->hide();

			// restore geometry
			// this shit already runs on event thread
			// so this is fine
			const_cast<slint::Window &>(newInst->window()).set_position(oldPos);

			// swap
			{
				std::lock_guard<std::mutex> lock(reloadMutex);
				definition = std::move(newDef);
				instance = newInst;
				weak_instance = newInst;
			}

		#ifdef _WIN32
			instance.value()->window().on_close_requested(
				[this]() -> slint::CloseRequestResponse
				{
#ifndef NDEBUG
					this->quit();
					return slint::CloseRequestResponse::HideWindow;
#else
					instance.value()->hide();
					return slint::CloseRequestResponse::KeepWindowShown;
#endif
				});

			HWND hwnd = FindWindowW(nullptr, L"HPR");
			if (hwnd)
			{
				HICON hIconBig =
					(HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), IMAGE_ICON, 32, 32, LR_SHARED);
				HICON hIconSmall =
					(HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), IMAGE_ICON, 16, 16, LR_SHARED);
				if (hIconBig)
					SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
				if (hIconSmall)
					SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
			}
		#else
			instance.value()->window().on_close_requested(
				[this]() -> slint::CloseRequestResponse
				{
#ifndef NDEBUG
					this->quit();
					return slint::CloseRequestResponse::HideWindow;
#else
					this->hide();
					return slint::CloseRequestResponse::KeepWindowShown;
#endif
				});
		#endif
		});
}

void HPRInterpreter::show()
{
	// tell to unpause
	{
		std::lock_guard<std::mutex> lock(pauseMutex);
		paused = false;
	}
	pauseCv.notify_one();

	auto weak = slint::ComponentWeakHandle<slint::interpreter::ComponentInstance>(instance.value());

	slint::invoke_from_event_loop(
		[weak]()
		{
			if (auto handle = weak.lock())
			{
				(*handle)->show();
#ifdef _WIN32
				HWND hwnd = FindWindowW(nullptr, L"HPR");
				if (hwnd)
				{
					HICON hIconBig =
						(HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), IMAGE_ICON, 32, 32, LR_SHARED);
					HICON hIconSmall =
						(HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), IMAGE_ICON, 16, 16, LR_SHARED);
					if (hIconBig)
						SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
					if (hIconSmall)
						SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
					ShowWindow(hwnd, SW_SHOW);
					ShowWindow(hwnd, SW_RESTORE);
					SetForegroundWindow(hwnd);
				}
#endif
			}
		});

	if (AppState::configManager.isFirstLaunch() && !AppState::configManager.isTelemetryPromptAnswered())
	{
		if (extManager)
		{
			std::string promptText = "Help Improve HPR? HPR is local-first. We would like to collect two anonymous numbers: total unique installations and the frequency of regular weekly users. No window titles, websites, or personal data ever leave your machine.";
			extManager->showUiPopup(promptText, "NO THANKS", "I AGREE", false, [](int btn) {
				if (btn == 1) // YES / OK
				{
					AppState::configManager.setConfig("anonymous-telemetry", std::string("true"));
					AppState::configManager.markTelemetryPromptAnswered();
					std::thread([]() { TelemetryManager::checkAndSend(); }).detach();
				}
				else // NO / CANCEL
				{
					AppState::configManager.setConfig("anonymous-telemetry", std::string("false"));
					AppState::configManager.markTelemetryPromptAnswered();
				}
			});
		}
	}
}

void HPRInterpreter::quit()
{
	saveWindowGeometry();
	slint::invoke_from_event_loop([]() { slint::quit_event_loop(); });
}

void HPRInterpreter::hide()
{
	// pause the thread
	{
		std::lock_guard<std::mutex> lock(pauseMutex);
		paused = true;
	}

	saveWindowGeometry();

	if (instance.has_value())
	{
		auto weak = slint::ComponentWeakHandle<slint::interpreter::ComponentInstance>(instance.value());
		slint::invoke_from_event_loop(
			[weak]()
			{
				if (auto handle = weak.lock())
				{
					(*handle)->hide();
				}
			});
	}
}

void HPRInterpreter::trackingLoop()
{

// This sets the title bar icon on windows
// 🖕 windows and microslop
#ifdef _WIN32
	HWND hwnd = FindWindowW(nullptr, L"HPR");
	if (hwnd)
	{
		HICON hIconBig = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), IMAGE_ICON, 32, 32, LR_SHARED);
		HICON hIconSmall = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), IMAGE_ICON, 16, 16, LR_SHARED);
		if (hIconBig)
			SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
		if (hIconSmall)
			SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
	}
#endif

	errorId = EventHub::connect(Event::APP_ERROR,
								[this](EventData data)
								{
									if (std::holds_alternative<ErrorGui>(data))
									{
										std::string error = std::get<ErrorGui>(data).error;
										activeGuiError = error;
										if (!error.empty())
										{
											errorTimestamp = std::chrono::steady_clock::now();
										}
									}
								});

	bool uiReady = false;

	// Stuff native to c++, holds raw values
	uint64_t totalTrackedTime; // For the bars
	uint64_t totalTrackedTime_Tab;
	uint64_t totalTrackedTime_Project;
	std::string window;
	std::map<std::string, uint64_t> timeLog;
	std::map<std::string, uint64_t> timeLog_Tab;
	std::map<std::string, uint64_t> timeLog_Project;
	std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;

	// Special priority to insights because they arent exactly on demand loaded,
	// rather updated every 5 minutes
	auto lastInsightUpdate = std::chrono::steady_clock::now();
	bool firstRun = true;

	while (running)
	{
		{
			std::unique_lock<std::mutex> lock(pauseMutex);

			pauseCv.wait(lock, [this] { return !paused || !running; });
		}

		// If the app was literally CLOSED while hidden, break out immediately
		if (!running)
			break;

		{
			auto now = std::chrono::steady_clock::now();

			totalTrackedTime = 0; // reset to 0 at every iteration
			totalTrackedTime_Tab = 0;
			totalTrackedTime_Project = 0;
			// Scoped mutex to hold it for as little time as possible
			{
				std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
				// If live view, show today's data
				if (AppState::state.currentView == AppState::CurrentView::LIVE)
				{
					window = AppState::state.currentWindow;
					timeLog = AppState::state.timeLog_PerApp;
					timeLog_Tab = AppState::state.timeLog_PerTab;
					timeLog_Project = AppState::state.timeLog_PerProject;
					switchHistory = AppState::state.switchHistory;
				}
				else if (AppState::state.currentView == AppState::CurrentView::HISTORICAL_SINGULAR)
				{
					std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
					// ALWAYS GET CURRENT LATEST WINDOW NO MATTER THE VIEW
					window = AppState::state.currentWindow;
					timeLog = AppState::historicalData_State.timeLog_PerApp;
					timeLog_Tab = AppState::historicalData_State.timeLog_PerTab;
					timeLog_Project = AppState::historicalData_State.timeLog_PerProject;
					switchHistory = AppState::historicalData_State.switchHistory;
				}
				else if (AppState::state.currentView == AppState::CurrentView::HISTORICAL_NUMBER ||
						 AppState::state.currentView == AppState::CurrentView::HISTORICAL_RANGE)
				{
					std::lock_guard<std::mutex> histLock(AppState::historyStateMutex);
					window = AppState::state.currentWindow;
					timeLog = AppState::historicalData_Full_State.timeLog_PerApp;
					timeLog_Tab = AppState::historicalData_Full_State.timeLog_PerTab;
					timeLog_Project = AppState::historicalData_Full_State.timeLog_PerProject;
					switchHistory = AppState::historicalData_Full_State.switchHistory;
				}
			}

			// Overwrite the window variable if we have an active error
			if (!activeGuiError.empty())
			{
				auto now = std::chrono::steady_clock::now();
				int errorDuration = AppState::configManager.getConfig("ui-error-duration", 5000);
				if (std::chrono::duration_cast<std::chrono::milliseconds>(now - errorTimestamp).count() >=
					errorDuration)
				{
					// clear the error
					activeGuiError = "";
				}
				else
				{
					// keep showing it
					window = "Error: " + activeGuiError;
				}
			}

			// Scoped mutex for hot reloading
			{
				std::lock_guard<std::mutex> lock(reloadMutex);
				modelManager.value().update_Interpreted(timeLog, timeLog_Tab, timeLog_Project, switchHistory, window,
														totalTrackedTime, totalTrackedTime_Tab,
														totalTrackedTime_Project, AppState::aliasManager);

				{
					std::lock_guard<std::mutex> lock(AppState::patternAnalyzerMutex);
					AppState::patternAnalyzer.generateInsights();
				}

				// Update insight (or on first frame)
				int insightInterval = AppState::configManager.getConfig("ui-insight-interval", 1000);
				if (firstRun ||
					std::chrono::duration_cast<std::chrono::milliseconds>(now - lastInsightUpdate).count() >=
						insightInterval)
				{
					std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);

					modelManager.value().showInsights_Interpreted(
						AppState::patternAnalyzer.getMostUsed(), AppState::patternAnalyzer.getTotalTrackedTime(),
						AppState::patternAnalyzer.getSwitchCount(), AppState::patternAnalyzer.getMostSwitchedFrom(),
						AppState::patternAnalyzer.getMostSwitchedTo(),
						AppState::patternAnalyzer.getMostFocusedSession(),
						AppState::patternAnalyzer.getMostProductiveHour(), AppState::patternAnalyzer.getEscapePattern(),
						AppState::patternAnalyzer.getReturnRate(), AppState::patternAnalyzer.getAvgFocusSession(),
						AppState::patternAnalyzer.getMostDistractedDay(),
						AppState::patternAnalyzer.getProductiveDaysThisWeek(),
						AppState::patternAnalyzer.getScreenTimeVsAverage(), AppState::patternAnalyzer.getFocusDipHour(),
						AppState::patternAnalyzer.getDeepWorkBeforeNoon(),
						AppState::patternAnalyzer.getWeekendVsWeekday());
					lastInsightUpdate = now;
					firstRun = false;
				}

				std::map<std::pair<std::string, std::string>, AppState::ExtensionInfo> extensionMapCopy;
				{
					std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
					extensionMapCopy = AppState::state.extensionMap;
				}
				modelManager.value().showExtensions_Interpreted(extensionMapCopy);
				modelManager.value().showFunStats_Interpreted(getCpuUsage(), getRamUsage(),
															  std::to_string(extensionMapCopy.size()), getThreadCount());
			}
		}

		if (!uiReady)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(400));
			uiReady = true;
			EventHub::emit(Event::UI_READY);
		}
		else
		{
			// Chunked sleep based on ui-update-interval
			int uiUpdateInterval = AppState::configManager.getConfig("ui-update-interval", 600);
			int remaining = uiUpdateInterval;
			while (remaining > 0 && running)
			{
				int sleepTime = std::min(remaining, 50);
				std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
				remaining -= sleepTime;
			}
		}

		saveWindowGeometry();
	}
}

void HPRInterpreter::run()
{

	// For saving my time
	auto &inst = instance.value();

	uiEventBridge.emplace(inst, extManager, this);

	int savedWidth = AppState::configManager.getConfig<int>("window-width", 1000);
	int savedHeight = AppState::configManager.getConfig<int>("window-height", 700);
	int savedX = AppState::configManager.getConfig<int>("window-pos-x", -1);
	int savedY = AppState::configManager.getConfig<int>("window-pos-y", -1);

	auto weak_inst = slint::ComponentWeakHandle<slint::interpreter::ComponentInstance>(inst);
	slint::invoke_from_event_loop(
		[weak_inst, savedWidth, savedHeight, savedX, savedY]()
		{
			if (auto handle = weak_inst.lock())
			{
				if (savedWidth > 200 && savedHeight > 200)
				{
					const_cast<slint::Window &>((*handle)->window()).set_size(slint::PhysicalSize(slint::Size<uint32_t>{static_cast<uint32_t>(savedWidth), static_cast<uint32_t>(savedHeight)}));
				}
				if (savedX >= 0 && savedY >= 0)
				{
					const_cast<slint::Window &>((*handle)->window()).set_position(slint::PhysicalPosition(slint::Point<int32_t>{savedX, savedY}));
				}
			}
		});

#ifdef _WIN32
	inst->window().on_close_requested(
		[this, weak_inst]() -> slint::CloseRequestResponse
		{
#ifndef NDEBUG
			this->quit();
			return slint::CloseRequestResponse::HideWindow;
#else
			saveWindowGeometry();
			if (auto locked = weak_inst.lock())
			{
				(*locked)->hide();
			}
			return slint::CloseRequestResponse::KeepWindowShown;
#endif
		});
#else
	inst->window().on_close_requested(
		[this]() -> slint::CloseRequestResponse
		{
#ifndef NDEBUG
			this->quit();
			return slint::CloseRequestResponse::HideWindow;
#else
			saveWindowGeometry();
			this->hide();
			return slint::CloseRequestResponse::KeepWindowShown;
#endif
		});
#endif

	bool headless = AppState::configManager.getConfig("headless-mode", false);
	if (!headless)
	{
		inst->show();
	}
	else
	{
		inst->show();
		auto weak = slint::ComponentWeakHandle<slint::interpreter::ComponentInstance>(inst);
		slint::invoke_from_event_loop(
			[weak]()
			{
				if (auto handle = weak.lock())
					(*handle)->hide();
			});
	}

#ifdef _WIN32
	// post to event loop so it runs AFTER the window is actually visible
	// cz when app is launched frshly, theres no icon
	slint::invoke_from_event_loop(
		[]()
		{
			HWND hwnd = FindWindowW(nullptr, L"HPR");
			if (hwnd)
			{
				HICON hIconBig =
					(HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), IMAGE_ICON, 32, 32, LR_SHARED);
				HICON hIconSmall =
					(HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), IMAGE_ICON, 16, 16, LR_SHARED);
				if (hIconBig)
					SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
				if (hIconSmall)
					SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
			}
		});
#endif

	tracker = std::thread(&HPRInterpreter::trackingLoop, this);

	slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
	saveWindowGeometry();
	running = false; // safety net
}

bool HPRInterpreter::initialiseSlintUiPath()
{
	std::filesystem::path tempPath;
#ifdef _WIN32
	tempPath = std::getenv("APPDATA");
	tempPath /= std::filesystem::path("HPR/HPR_Config/ui/");
#else
	const char *home = std::getenv("HOME");
	if (!home)
		throw std::runtime_error("HOME env var not set");
	tempPath = home;
	tempPath /= std::filesystem::path(".config/HPR/ui/");
#endif

	std::filesystem::create_directories(tempPath);
	filePath = tempPath;

	std::ifstream file(filePath / fileName);

	if (!file.is_open())
	{
		std::cerr << "Warning: " << fileName << "  not found at " << filePath.string() << ". Closing HPR .\n";
		Logger::log("Warning: " + fileName + " not found at " + filePath.string() + ". Closing HPR.");
		return false;
	}
	return true;
}

void HPRInterpreter::setUiImage(const std::string &propertyName,
								const slint::SharedPixelBuffer<slint::Rgba8Pixel> &pixelBuffer)
{
	if (!instance.has_value())
		return;
	slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> weak(instance.value());
	slint::invoke_from_event_loop(
		[weak, propertyName, pixelBuffer]()
		{
			if (auto handle = weak.lock())
			{
				if ((*handle)->get_property(propertyName).has_value())
				{
					slint::Image img(pixelBuffer);
					(*handle)->set_property(propertyName, slint::interpreter::Value(img));
				}
			}
		});
}