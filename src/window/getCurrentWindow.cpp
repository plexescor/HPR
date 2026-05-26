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

	if (currentPlatform.contains("KDE"))
	{
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
	}
	
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
			// std::cout << lowerWindowName << std::endl;
			tab = getCurrentTab();
			// std::cout << tab << std::endl;
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

			std::string lowerTabName = tab;

			//Convert windowName to lowercase
			std::transform(lowerTabName.begin(), 
				lowerTabName.end(), 
				lowerTabName.begin(),
				[](unsigned char c)
				{ 
					return std::tolower(c); 
				});

			if (!tab.empty() && (lowerTabName.contains("chrome") || lowerTabName.contains("edge") || lowerTabName.contains("firefox") || lowerTabName.contains("brave")))
			{
				// std::cout << tab << std::endl;
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

	else if (currentPlatform.contains("Cinnamon"))
	{
		window = getCurrentWindow_Cinnamon();
	}

	// Need to explicitly set this because shit happens if cached value is
	// returned
	window = validateAndUpdateWindow_Cross(window);
	return window;
}

std::string CurrentWindowManager::getCurrentWindow_Hyprland()
{
	std::string cmd = "hyprctl activewindow -j";
    std::string json = runSystemCommand(cmd);
    
    // Find "class":"value"
    const std::string key = "\"class\":";
    size_t keyPos = json.find(key);
    if (keyPos == std::string::npos) return "";
    
    size_t quoteStart = json.find('"', keyPos + key.size());
    if (quoteStart == std::string::npos) return "";
    
    size_t quoteEnd = json.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos) return "";
    
    return json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
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

std::string CurrentWindowManager::getCurrentWindow_Cinnamon()
{
	std::string cmd = "gdbus call --session --dest org.Cinnamon --object-path /org/Cinnamon --method org.Cinnamon.Eval \"global.display.focus_window.get_wm_class()\"";
	std::string rawOutput = runSystemCommand(cmd);
	// Find the positions of the single-quotes wrapping the inner string
	size_t startQuote = rawOutput.find('\'');
	size_t endQuote = rawOutput.rfind('\'');
	
	if (startQuote == std::string::npos || endQuote == std::string::npos || startQuote >= endQuote) {
		return ""; // return empty if quotes aren't matched
	}
	
	// Extract everything between the single quotes
	std::string inner = rawOutput.substr(startQuote + 1, endQuote - startQuote - 1);
	
	// strip the literal double-quotes if they exist
	if (inner.length() >= 2 && inner.front() == '"' && inner.back() == '"') {
		inner = inner.substr(1, inner.length() - 2);
	}
	
	// trim any trailing newlines or extra whitespaces
	while (!inner.empty() && (inner.back() == '\n' || inner.back() == '\r' || inner.back() == ' ')) {
		inner.pop_back();
	}

	return inner;
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

	else if (currentPlatform.contains("Cinnamon"))
	{
		tab = getCurrentTab_Cinnamon();
	}

	// Need to explicitly set this because shit happens if cached value is
	// returned
	// tab = validateAndUpdateWindow_Cross(window);
	// std::cout << tab << std::endl;
	return tab;
}

std::string CurrentWindowManager::getCurrentTab_Hyprland()
{
	std::string cmd = "hyprctl activewindow -j";
    std::string json = runSystemCommand(cmd);
    
    // Find "title":"value"
    const std::string key = "\"title\":";
    size_t keyPos = json.find(key);
    if (keyPos == std::string::npos) return "";
    
    size_t quoteStart = json.find('"', keyPos + key.size());
    if (quoteStart == std::string::npos) return "";
    
    size_t quoteEnd = json.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos) return "";
    
    return json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
}

std::string CurrentWindowManager::getCurrentTab_Windows()
{
	#ifdef _WIN32
		HWND hwnd = GetForegroundWindow();
		if (!hwnd)
			return "";

		wchar_t title[512] = {};
		GetWindowTextW(hwnd, title, sizeof(title) / sizeof(wchar_t));

		std::wstring wstr(title);
		if (wstr.empty())
			return "";

		int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
		std::string strTo(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);

		return strTo;
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

std::string CurrentWindowManager::getCurrentTab_Cinnamon()
{
	std::string cmd = "gdbus call --session --dest org.Cinnamon --object-path /org/Cinnamon --method org.Cinnamon.Eval 'global.get_window_actors().filter(a => a.meta_window.has_focus())[0].get_meta_window().get_title()'";
	
	std::string rawOutput = runSystemCommand(cmd);

	// Find the positions of the single-quotes wrapping the inner string
    size_t startQuote = rawOutput.find('\'');
    size_t endQuote = rawOutput.rfind('\'');
    
    if (startQuote == std::string::npos || endQuote == std::string::npos || startQuote >= endQuote) {
        return ""; // return empty if quotes aren't matched
    }
    
    // Extract everything between the single quotes
    std::string inner = rawOutput.substr(startQuote + 1, endQuote - startQuote - 1);
    
    // strip the literal double-quotes if they exist
    if (inner.length() >= 2 && inner.front() == '"' && inner.back() == '"') {
        inner = inner.substr(1, inner.length() - 2);
    }
    
    // trim any trailing newlines or extra whitespaces
    while (!inner.empty() && (inner.back() == '\n' || inner.back() == '\r' || inner.back() == ' ')) {
        inner.pop_back();
    }

    return inner;
}