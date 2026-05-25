#include <iostream>
#include <string>

#include "appState.hpp"
#include "getCurrentWindow.hpp"
#include "validateAndUpdateWindow.hpp"
#include "windowUtilities.hpp"

#ifdef _WIN32 // Include windows headers
#include <windows.h>
#include <psapi.h>
#endif

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <filesystem>

CurrentWindowManager::CurrentWindowManager()
{
#ifdef __linux__
	// Get current desktop environment, linux only
	std::string currentPlatformCommand = "echo $XDG_CURRENT_DESKTOP";
	currentPlatform = runSystemCommand(currentPlatformCommand);

	// Trim newline from platform string
	if (!currentPlatform.empty() && currentPlatform.back() == '\n')
		currentPlatform.pop_back();

	std::cout << "[HPR] Detected platform: " << currentPlatform << std::endl;


	{
		std::lock_guard<std::mutex> lock(AppState::stateMutex);
		AppState::state.currentPlatform = currentPlatform;
	}

	// resolve the correct qdbus binary for this distro
	// thanks https://github.com/tempodat
	// arch/most distros use qdbus6, Fedora ships it as qdbus-qt6, some have plain qdbus
	std::string resolveQdbus = "command -v qdbus6 || command -v qdbus-qt6 || command -v qdbus";
	qdbusCmd = runSystemCommand(resolveQdbus);
	if (!qdbusCmd.empty() && qdbusCmd.back() == '\n')
		qdbusCmd.pop_back();
	if (qdbusCmd.empty())
		qdbusCmd = "qdbus6"; // fallback
	std::cout << "[HPR] Using qdbus binary: " << qdbusCmd << std::endl;
	

	
	if (currentPlatform.contains("GNOME"))
	{
		// Check if extension files already exist on disk
		std::string extDir = std::string(getenv("HOME")) + 
			"/.local/share/gnome-shell/extensions/window-calls-extended@hseliger.eu";
		
		bool filesExist = std::filesystem::exists(extDir);

		// Check if window-calls-extended is working
		std::string checkCmd =
			"gdbus call --session --dest org.gnome.Shell --object-path "
			"/org/gnome/Shell/Extensions/WindowsExt --method "
			"org.gnome.Shell.Extensions.WindowsExt.FocusClass 2>&1";
		std::string checkResult = runSystemCommand(checkCmd);

		std::cout << "[HPR] Extension check result: " << checkResult << std::endl;

		if (!checkResult.contains("('"))
		{
			if (filesExist)
			{
				std::cout << "[HPR] Extension files found, enabling..." << std::endl;
				std::string cmd = "gnome-extensions enable window-calls-extended@hseliger.eu";
				runSystemCommand(cmd);
				currentPlatform = "GNOME";
			}
			else
			{
				currentPlatform = "GNOME_NO_EXTENSION";
			}
		}

		else
		{
			std::cout << "[HPR] Extension working, proceeding normally" << std::endl;
		}
	}
#endif

#ifdef _WIN32
	currentPlatform = "Windows";
#endif
}

CurrentWindowManager::~CurrentWindowManager()
{
	running = false;

	if (windowPollingThread.joinable())
		windowPollingThread.join();
}

void CurrentWindowManager::run()
{
	std::cout << "Running Loop!\n";

	windowPollingThread =
		std::thread(&CurrentWindowManager::getCurrentWindow_Loop, this);
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
			std::lock_guard<std::mutex> lock(AppState::stateMutex);
			AppState::state.currentWindow = window;
			continue;
		}
		if (window.contains("Unknown"))
		{
			continue;
		}

		//Tab
		if (lowerWindowName.contains("chrome") 
		|| lowerWindowName.contains("edge")
		|| lowerWindowName.contains("firefox")
		|| lowerWindowName.contains("brave"))
		{
			tab = getCurrentTab();
		}

		else
		{
			tab = "";
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

			if (!tab.empty())
			{
				AppState::state.timeLog_PerTab[tab] +=
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

	if (currentPlatform.contains("Hyprland"))
	{
		window = getCurrentWindow_Hyprland();
	}

	else if (currentPlatform.contains("Windows"))
	{
		window = getCurrentWindow_Windows();
	}

	else if (currentPlatform.contains("GNOME_NO_EXTENSION"))
	{
		// This means we need to prompt the user to restart
		// Return immediately

		return "RUN THE \"installWindowCallsExtension.sh\" SCRIPT NEXT TO THE HPR "
			   "BINARY AND THE RESTART PC";
	}

	else if (currentPlatform.contains("GNOME")) // Motherfucking GNOME
	{
		window = getCurrentWindow_Gnome();
	}

	else if (currentPlatform.contains("KDE"))
	{
		window = getCurrentWindow_KDE();
	}

	// Need to explicitly set this because shit happens if cached value is
	// returned
	window = validateAndUpdateWindow_Cross(window);
	return window;
}

std::string CurrentWindowManager::getCurrentWindow_Hyprland()
{
	std::string command = "hyprctl activewindow -j | jq -r '.class'";

	// std::cout << "getcurrhypr: " << runSystemCommand(command) << std::endl;
	return runSystemCommand(command);
}

std::string CurrentWindowManager::getCurrentWindow_Windows()
{
#ifdef _WIN32
	HWND hwnd = GetForegroundWindow();
	if (!hwnd)
		return "";

	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (!pid)
		return "";

	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!hProcess)
		return "";

	char procName[MAX_PATH] = {};
	DWORD size = MAX_PATH;
	QueryFullProcessImageNameA(hProcess, 0, procName, &size);
	CloseHandle(hProcess);

	// Extract just the filename from full path
	std::string fullPath(procName);
	size_t lastSlash = fullPath.find_last_of("\\/");
	std::string filename = (lastSlash != std::string::npos) ? fullPath.substr(lastSlash + 1) : fullPath;

	// Strip .exe
	if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".exe")
		filename = filename.substr(0, filename.size() - 4);

	return filename;
#endif
	return "";
}
std::string CurrentWindowManager::getCurrentWindow_Gnome()
{
	std::string command =
		"gdbus call --session --dest org.gnome.Shell --object-path "
		"/org/gnome/Shell/Extensions/WindowsExt --method "
		"org.gnome.Shell.Extensions.WindowsExt.FocusClass";
	std::string result = runSystemCommand(command);

	// Parse the fucking dirty output
	size_t start = result.find('\'');
	size_t end = result.rfind('\'');
	if (start != std::string::npos && end != std::string::npos && start != end)
		return result.substr(start + 1, end - start - 1);

	return "";
}

std::string CurrentWindowManager::getCurrentWindow_KDE()
{
	std::string cmd =
		"echo 'print(workspace.activeWindow.resourceClass);' > /tmp/kwin_active.js && "
		"S=$(" + qdbusCmd + " org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript /tmp/kwin_active.js kwin_tmp_$$) && "
		"T=$(date '+%Y-%m-%d %H:%M:%S') && "
		+ qdbusCmd + " org.kde.KWin /Scripting/Script$S org.kde.kwin.Script.run > /dev/null 2>&1 && "
		"sleep 0.1 && "
		"journalctl --since \"$T\" -o cat | grep '^js:' | tail -n 1 | sed 's/^js: //' ; "
		+ qdbusCmd + " org.kde.KWin /Scripting/Script$S org.kde.kwin.Script.stop > /dev/null 2>&1 ; "
		+ qdbusCmd + " org.kde.KWin /Scripting unloadScript kwin_tmp_$$ > /dev/null 2>&1";

	std::string result = runSystemCommand(cmd);

	// Trim trailing newline/whitespace
	while (!result.empty() && (result.back() == '\n' || result.back() == '\r' ||
							   result.back() == ' '))
		result.pop_back();

	// Strip "js: " prefix that KWin journals print() output with
	const std::string prefix = "js: ";
	if (result.starts_with(prefix))
		result = result.substr(prefix.size());

	return result;
}

//--------------------TABS------------------------------
std::string CurrentWindowManager::getCurrentTab()
{

	if (currentPlatform.contains("Hyprland"))
	{
		tab = getCurrentTab_Hyprland();
	}

	else if (currentPlatform.contains("Windows"))
	{
		tab = getCurrentTab_Windows();
	}

	else if (currentPlatform.contains("GNOME_NO_EXTENSION"))
	{
		// This means we need to prompt the user to restart
		// Return immediately

		return "RUN THE \"installWindowCallsExtension.sh\" SCRIPT NEXT TO THE HPR "
			   "BINARY AND THE RESTART PC";
	}

	else if (currentPlatform.contains("GNOME")) // Motherfucking GNOME
	{
		tab = getCurrentTab_Gnome();
	}

	else if (currentPlatform.contains("KDE"))
	{
		tab = getCurrentTab_KDE();
	}

	// Need to explicitly set this because shit happens if cached value is
	// returned
	// tab = validateAndUpdateWindow_Cross(window);
	std::cout << tab << std::endl;
	return tab;
}

std::string CurrentWindowManager::getCurrentTab_Hyprland()
{
	std::string command = "hyprctl activewindow -j | jq -r '.title'";

	// std::cout << "getcurrhypr: " << runSystemCommand(command) << std::endl;
	return runSystemCommand(command);
}

std::string CurrentWindowManager::getCurrentTab_Windows()
{
	#ifdef _WIN32
		HWND hwnd = GetForegroundWindow();
		if (!hwnd)
			return "";

		char title[512] = {};
		GetWindowTextA(hwnd, title, sizeof(title));

		return std::string(title);
	#endif
		return "";
}
std::string CurrentWindowManager::getCurrentTab_Gnome()
{
	std::string command =
		"gdbus call --session --dest org.gnome.Shell --object-path "
		"/org/gnome/Shell/Extensions/WindowsExt --method "
		"org.gnome.Shell.Extensions.WindowsExt.FocusTitle";
	std::string result = runSystemCommand(command);

	// Parse the fucking dirty output
	size_t start = result.find('\'');
	size_t end = result.rfind('\'');
	if (start != std::string::npos && end != std::string::npos && start != end)
		return result.substr(start + 1, end - start - 1);

	return "";
}

std::string CurrentWindowManager::getCurrentTab_KDE()
{
	std::string cmd =
		"echo 'print(workspace.activeWindow.caption);' > /tmp/kwin_active.js && "
		"S=$(" + qdbusCmd + " org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript /tmp/kwin_active.js kwin_tmp_$$) && "
		"T=$(date '+%Y-%m-%d %H:%M:%S') && "
		+ qdbusCmd + " org.kde.KWin /Scripting/Script$S org.kde.kwin.Script.run > /dev/null 2>&1 && "
		"sleep 0.1 && "
		"journalctl --since \"$T\" -o cat | grep '^js:' | tail -n 1 | sed 's/^js: //' ; "
		+ qdbusCmd + " org.kde.KWin /Scripting/Script$S org.kde.kwin.Script.stop > /dev/null 2>&1 ; "
		+ qdbusCmd + " org.kde.KWin /Scripting unloadScript kwin_tmp_$$ > /dev/null 2>&1";

	std::string result = runSystemCommand(cmd);

	// Trim trailing newline/whitespace
	while (!result.empty() && (result.back() == '\n' || result.back() == '\r' ||
							   result.back() == ' '))
		result.pop_back();

	// Strip "js: " prefix that KWin journals print() output with
	const std::string prefix = "js: ";
	if (result.starts_with(prefix))
		result = result.substr(prefix.size());

	return result;
}
