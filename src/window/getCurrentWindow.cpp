#include <iostream>
#include <string>
#include <ranges>

#include "appState.hpp"
#include "getCurrentWindow.hpp"
#include "validateAndUpdateWindow.hpp"
#include "windowUtilities.hpp"
#include "builtinBackends.hpp"
#include "appEvents.hpp"
#include "logger.hpp"

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <filesystem>

CurrentWindowManager::CurrentWindowManager()
{
	registerBuiltinBackends();
}

CurrentWindowManager::~CurrentWindowManager()
{
	running = false;

	if (windowPollingThread.joinable())
		windowPollingThread.join();
}

void CurrentWindowManager::run()
{
	running = true;
	windowPollingThread =
        std::thread(
            &CurrentWindowManager::getCurrentWindow_Loop,
            this
        );
}

void CurrentWindowManager::stopTracking()
{
	if (running)
	{
		{
		std::lock_guard<std::mutex> lock(AppState::stateMutex);
		AppState::state.currentWindow = "AFK";
		}

		running = false;

		if (windowPollingThread.joinable())
			windowPollingThread.join();

	}
	
}

void CurrentWindowManager::startTracking()
{
	if (!running) run();
}

// Responsible for calling getCurrentWindow() and setting the result to AppState
// (state) struct so shit can access that
void CurrentWindowManager::getCurrentWindow_Loop()
{
	auto lastTimestamp = std::chrono::steady_clock::now(); // Get the tick's time
	window = getCurrentWindow();
	previousWindow = window;

	// Record initial arrival so the first app session can be counted
	{
		std::lock_guard<std::mutex> lock(AppState::stateMutex);
		auto nowSystem = std::chrono::system_clock::now();
		uint64_t t = std::chrono::duration_cast<std::chrono::milliseconds>(
						 nowSystem.time_since_epoch())
						 .count();
		AppState::state.switchHistory[{"Unknown", window}].push_back(t);
	}
	while (running)
	{
		int pollInterval = AppState::configManager.getConfig("poll-interval", 50);
		window = getCurrentWindow();

		std::string lowerWindowName = window;

		//Convert windowName to lowercase
		std::transform(lowerWindowName.begin(), 
			lowerWindowName.end(), 
			lowerWindowName.begin(),
			[](unsigned char c)
			{ 
				return std::tolower(c); 
			});

		if (window.contains("SCRIPT"))
		{
			{
				std::lock_guard<std::mutex> lock(AppState::stateMutex);
				AppState::state.currentWindow = window;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(pollInterval));
			continue;
		}
		if (window.contains("Unknown"))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(pollInterval));
			continue;
		}

		{
			std::lock_guard<std::mutex> lock(AppState::stateMutex);
			AppState::state.currentTitle = getCurrentTitle();
		}
		
		if (lowerWindowName.contains("code") 
		|| lowerWindowName.contains("vscode")
		|| lowerWindowName.contains("visual studio code"))
		{
			project = getCurrentTitle();
		}

		//title
		else if (lowerWindowName.contains("chrome") 
		|| lowerWindowName.contains("edge")
		|| lowerWindowName.contains("firefox")
		|| lowerWindowName.contains("brave"))
		{
			tab = getCurrentTitle();
		}

		else
		{
			tab = "";
			project = "";
		}

		bool windowChanged = false;
		std::string emitPrevWindow;
		std::string emitCurrWindow;
		std::string pid = getCurrentPid();

		{
			// Update current window in the AppState
			std::lock_guard<std::mutex> lock(AppState::stateMutex);
			AppState::state.currentWindow = window;

			auto now = std::chrono::steady_clock::now(); // get time now

			if (previousWindow != window)
			{
				// This means window was changed
				// FromWindow = previousWindow, ToWindow = window

				windowChanged = true;
				emitPrevWindow = previousWindow;
				emitCurrWindow = window;

				auto nowSystem = std::chrono::system_clock::now();
				uint64_t t = std::chrono::duration_cast<std::chrono::milliseconds>(
								 nowSystem.time_since_epoch())
								 .count();

				AppState::state.switchHistory[{previousWindow, window}].push_back(t);

				previousWindow = window;
			}

			AppState::state.previousWindow = previousWindow;

			auto elapsed = now - lastTimestamp;
			lastTimestamp = now;
			AppState::state.timeLog_PerApp[window] +=
				std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
					.count();

			std::string lowerTabName = tab;

			//Convert windowName to lowercase
			std::transform(lowerTabName.begin(), 
				lowerTabName.end(), 
				lowerTabName.begin(),
				[](unsigned char c)
				{ 
					return std::tolower(c); 
				});

			std::string lowerProjectName = project;

			//Convert windowName to lowercase
			std::transform(lowerProjectName.begin(), 
				lowerProjectName.end(), 
				lowerProjectName.begin(),
				[](unsigned char c)
				{ 
					return std::tolower(c); 
				});

			if (!tab.empty() && (lowerTabName.contains("chrome") || lowerTabName.contains("edge") || lowerTabName.contains("firefox") || lowerTabName.contains("brave")))
			{
				AppState::state.timeLog_PerTab[tab] +=
				std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
					.count();
			}
			
			if (!project.empty() && (lowerProjectName.contains("visual studio code") || lowerProjectName.contains("vscode") || lowerProjectName.contains("code")))
			{
				AppState::state.timeLog_PerProject[project] +=
				std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
					.count();
			}

			AppState::state.appNamePid[window] = pid;
		}

		if (windowChanged)
		{
			EventHub::emit(Event::WINDOW_CHANGED, WindowChangedData{emitPrevWindow, emitCurrWindow});
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(pollInterval));
	}
}

// A singular function to return currently active window. Does platform specific
// calling and validating automatically
std::string CurrentWindowManager::getCurrentWindow()
{

	if (!activeBackend)
        return "No backend";

	std::string curr = activeBackend->getCurrentWindow();
    return validateAndUpdateWindow_Cross(curr);
}

std::string CurrentWindowManager::getCurrentPid()
{

	if (!activeBackend)
        return "No backend";

	std::string curr = activeBackend->getCurrentPid();
	return curr;
}

std::string CurrentWindowManager::getCurrentTitle()
{
	if (!activeBackend)
        return "No backend";

	std::string curr = activeBackend->getCurrentTitle();
    return validateAndUpdateWindow_Cross(curr);
}void CurrentWindowManager::detectAndSetBackend()
{
#ifdef __linux__

	std::string cmd = "echo $XDG_CURRENT_DESKTOP";

    std::string xdg = runSystemCommand(cmd);
    
	currentPlatform = xdg;

	Logger::log("XDG_CURRENT_DESKTOP = '" + xdg + "'");
    Logger::log("Detected platform = '" + currentPlatform + "'");
#endif

#ifdef _WIN32
    currentPlatform = "Windows";
#endif
#ifdef __APPLE__
    currentPlatform = "Apple";
#endif

    Logger::log("Detected environment: " + currentPlatform);

    for (auto& backend : std::views::reverse(registeredBackends))
    {
        if (!backend.matchesEnvironment(currentPlatform)) continue;
        Logger::log("Trying backend: " + backend.name);
        backend.initialize();
        bool usable = false;
        auto startTime = std::chrono::steady_clock::now();
        while (true)
        {
            if (backend.isUsable())
            {
                usable = true;
                break;
            }
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime
            ).count();
            if (elapsed >= 7000)
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        if (!usable)
        {
            Logger::log("Backend unusable: " + backend.name);
            continue;
        }
        activeBackend = &backend;
        Logger::log("Selected backend: " + backend.name);
        return;
    }
    Logger::log("Failed to find usable backend");

    Logger::log("Failed to find usable backend");
} 