#include "app-window.h"

#include "HPR.hpp"
#include "aliasManager.hpp"
#include "appEvents.hpp"
#include "appState.hpp"
#include "currentWindowManager.hpp"
#include "patternAnalyzer.hpp"
#include "timeUtils.hpp"
#include "uiEventBridge.hpp"
#include "windowUtilities.hpp"
#include "telemetryManager.hpp"

// Slint stuff
#include <slint.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <print>
#include <thread>
#include <vector>

HPR::HPR(ExtensionManager *extMgr) : ui(MainWindow::create()), modelManager(ui)
{
	if (extMgr)
		this->extManager = extMgr;
#ifdef __linux__
	slint::set_xdg_app_id("HPR"); // So it has a class in hyprland
#endif
}

void HPR::saveWindowGeometry()
{
	auto weak = slint::ComponentWeakHandle<MainWindow>(ui);
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

HPR::~HPR()
{
	running = false;

	// Wake up the thread if it's trapped in a hidden pause cycle during
	// shutdown
	pauseCv.notify_all();

	if (tracker.joinable())
		tracker.join(); // Speaks for itself

	EventHub::disconnect(Event::APP_ERROR, errorId);
}

void HPR::show()
{
	// tell to unpause
	{
		std::lock_guard<std::mutex> lock(pauseMutex);
		paused = false;
	}
	pauseCv.notify_one();

	auto weak = slint::ComponentWeakHandle<MainWindow>(ui);

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
			extManager->showUiPopup(promptText, "NO THANKS", "I AGREE", [](int btn) {
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

void HPR::quit()
{
	saveWindowGeometry();
	slint::invoke_from_event_loop([]() { slint::quit_event_loop(); });
}

void HPR::hide()
{
	// pause the thread
	{
		std::lock_guard<std::mutex> lock(pauseMutex);
		paused = true;
	}

	saveWindowGeometry();

	auto weak = slint::ComponentWeakHandle<MainWindow>(ui);
	slint::invoke_from_event_loop(
		[weak]()
		{
			if (auto handle = weak.lock())
			{
				(*handle)->hide();
			}
		});
}

void HPR::trackingLoop()
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
	uint64_t totalTrackedTime;
	uint64_t totalTrackedTime_Tab;
	uint64_t totalTrackedTime_Project;
	std::string window;
	std::map<std::string, uint64_t> timeLog;
	std::map<std::string, uint64_t> timeLog_Tab;
	std::map<std::string, uint64_t> timeLog_Project;
	std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;

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
					std::lock_guard<std::mutex> histLock(AppState::historyStateMutex);
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

			modelManager.update(timeLog, timeLog_Tab, timeLog_Project, switchHistory, window, totalTrackedTime,
								totalTrackedTime_Tab, totalTrackedTime_Project, AppState::aliasManager);

			{
				std::lock_guard<std::mutex> lock(AppState::patternAnalyzerMutex);
				AppState::patternAnalyzer.generateInsights();
			}

			// Update insight (or on first frame)
			int insightInterval = AppState::configManager.getConfig("ui-insight-interval", 1000);
			if (firstRun || std::chrono::duration_cast<std::chrono::milliseconds>(now - lastInsightUpdate).count() >=
								insightInterval)
			{
				std::lock_guard<std::mutex> lock(AppState::patternAnalyzerMutex);

				modelManager.showInsights(
					AppState::patternAnalyzer.getMostUsed(), AppState::patternAnalyzer.getTotalTrackedTime(),
					AppState::patternAnalyzer.getSwitchCount(), AppState::patternAnalyzer.getMostSwitchedFrom(),
					AppState::patternAnalyzer.getMostSwitchedTo(), AppState::patternAnalyzer.getMostFocusedSession(),
					AppState::patternAnalyzer.getMostProductiveHour(), AppState::patternAnalyzer.getEscapePattern(),
					AppState::patternAnalyzer.getReturnRate(), AppState::patternAnalyzer.getAvgFocusSession(),
					AppState::patternAnalyzer.getMostDistractedDay(),
					AppState::patternAnalyzer.getProductiveDaysThisWeek(),
					AppState::patternAnalyzer.getScreenTimeVsAverage(), AppState::patternAnalyzer.getFocusDipHour(),
					AppState::patternAnalyzer.getDeepWorkBeforeNoon(), AppState::patternAnalyzer.getWeekendVsWeekday());
				lastInsightUpdate = now;
				firstRun = false;
			}

			std::vector<std::pair<std::string, std::string>> extensionsCopy;
			{
				std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
				extensionsCopy = AppState::state.loadedExtensions;
			}
			modelManager.showExtensions(extensionsCopy);
			modelManager.showFunStats(getCpuUsage(), getRamUsage(), std::to_string(extensionsCopy.size()),
									  getThreadCount());
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

void HPR::run()
{
	UiEventBridge uiEventBridge(ui, extManager);

	int savedWidth = AppState::configManager.getConfig<int>("window-width", 1000);
	int savedHeight = AppState::configManager.getConfig<int>("window-height", 700);
	int savedX = AppState::configManager.getConfig<int>("window-pos-x", -1);
	int savedY = AppState::configManager.getConfig<int>("window-pos-y", -1);

	auto weak = slint::ComponentWeakHandle<MainWindow>(ui);
	slint::invoke_from_event_loop(
		[weak, savedWidth, savedHeight, savedX, savedY]()
		{
			if (auto handle = weak.lock())
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
	ui->window().on_close_requested(
		[this]() -> slint::CloseRequestResponse
		{
			saveWindowGeometry();
			ui->hide();
			return slint::CloseRequestResponse::KeepWindowShown;
		});
#else
	// same as windows, X button just hides to tray
	ui->window().on_close_requested(
		[this]() -> slint::CloseRequestResponse
		{
			saveWindowGeometry();
			ui->hide();
			return slint::CloseRequestResponse::KeepWindowShown;
		});
#endif

	bool headless = AppState::configManager.getConfig("headless-mode", false);
	if (!headless)
	{
		std::println("Showing ui");
		ui->show();
	}
	else
	{
		ui->show();
		auto weak = slint::ComponentWeakHandle<MainWindow>(ui);
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

	tracker = std::thread(&HPR::trackingLoop, this);
	slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
	saveWindowGeometry();
	running = false; // safety net
}

void HPR::setUiImage(const std::string &propertyName, const slint::SharedPixelBuffer<slint::Rgba8Pixel> &pixelBuffer)
{
	auto weak = slint::ComponentWeakHandle<MainWindow>(ui);
	slint::invoke_from_event_loop(
		[weak, propertyName, pixelBuffer]()
		{
			if (auto handle = weak.lock())
			{
				if (propertyName == "miscImage_S")
				{
					slint::Image img(pixelBuffer);
					(*handle)->set_miscImage_S(img);
				}
			}
		});
}