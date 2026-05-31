#include <iostream>
#include <string>
#include <ranges>

#include "appState.hpp"
#include "getCurrentWindow.hpp"
#include "validateAndUpdateWindow.hpp"
#include "windowUtilities.hpp"
#include "builtinBackends.hpp"
#include "appEvents.hpp"
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
	// std::cout << "Running Loop!\n";

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
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}
		if (window.contains("Unknown"))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
			// std::cout << lowerWindowName << std::endl;
			tab = getCurrentTitle();
			// std::cout << title << std::endl;
		}

		else
		{
			tab = "";
			project = "";
		}

		{
			// Update current window in the AppState
			std::lock_guard<std::mutex> lock(AppState::stateMutex);
			AppState::state.currentWindow = window;

			auto now = std::chrono::steady_clock::now(); // get time now

			if (previousWindow != window)
			{
				// This means window was changed
				// FromWindow = previousWindow, ToWindow = window

				EventHub::emit(Event::WINDOW_CHANGED, WindowChangedData{previousWindow, window});

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
				// std::cout << title << std::endl;
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
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
#endif

#ifdef _WIN32
    currentPlatform = "Windows";
#endif
#ifdef __APPLE__
    currentPlatform = "Apple";
#endif

    std::cout << "[HPR] Detected environment: " << currentPlatform << std::endl;

    for (auto& backend : std::views::reverse(registeredBackends))
    {
        if (!backend.matchesEnvironment(currentPlatform)) continue;
        std::cout << "[HPR] Initializing backend: " << backend.name << std::endl;
        backend.initialize();
        if (!backend.isUsable())
        {
            std::cout << "[HPR] Backend unusable: " << backend.name << std::endl;
            continue;
        }
        activeBackend = &backend;
        std::cout << "[HPR] Selected backend: " << backend.name << std::endl;
        return;
    }

    std::cout << "[HPR] Failed to find usable backend" << std::endl;
} 

// 	{
// 		std::lock_guard<std::mutex> lock(AppState::stateMutex);
// 		AppState::state.currentPlatform = currentPlatform;
// 	}

// 	if (currentPlatform.contains("KDE"))
// 	{
// 		// resolve the correct qdbus binary for this distro
// 		// thanks https://github.com/tempodat
// 		// arch/most distros use qdbus6, Fedora ships it as qdbus-qt6, some have plain qdbus
// 		std::string resolveQdbus = "command -v qdbus6 || command -v qdbus-qt6 || command -v qdbus";
// 		qdbusCmd = runSystemCommand(resolveQdbus);
// 		if (!qdbusCmd.empty() && qdbusCmd.back() == '\n')
// 			qdbusCmd.pop_back();
// 		if (qdbusCmd.empty())
// 			qdbusCmd = "qdbus6"; // fallback
// 		std::cout << "[HPR] Using qdbus binary: " << qdbusCmd << std::endl;
// 	}
	
// 	if (currentPlatform.contains("GNOME"))
// 	{
// 		// Check if extension files already exist on disk
// 		std::string extDir = std::string(getenv("HOME")) + 
// 			"/.local/share/gnome-shell/extensions/lol-another-window-extension@plexescor";
		
// 		bool filesExist = std::filesystem::exists(extDir);

// 		// Check if window-calls-extended is working
// 		std::string checkCmd =
// 			"gdbus call --session --dest org.gnome.Shell --object-path "
// 			"/org/gnome/Shell/Extensions/LolAnotherWindowExtension --method "
// 			"org.gnome.Shell.Extensions.LolAnotherWindowExtension.FocusClass 2>&1";
// 		std::string checkResult = runSystemCommand(checkCmd);

// 		std::cout << "[HPR] Extension check result: " << checkResult << std::endl;

// 		if (!checkResult.contains("('"))
// 		{
// 			if (filesExist)
// 			{
// 				std::cout << "[HPR] Extension files found, enabling..." << std::endl;
// 				std::string cmd = "gnome-extensions enable lol-another-window-extension@plexescor";
// 				runSystemCommand(cmd);
// 				currentPlatform = "GNOME";
// 			}
// 			else
// 			{
// 				currentPlatform = "GNOME_NO_EXTENSION";
// 			}
// 		}

// 		else
// 		{
// 			std::cout << "[HPR] Extension working, proceeding normally" << std::endl;
// 		}
// 	}
// #endif
