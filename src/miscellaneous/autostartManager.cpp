// autostartManager.cpp
#include "autostartManager.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#include <sys/stat.h>
#endif

bool AutostartManager::isEnabled()
{
#ifdef _WIN32
	HKEY hKey;
	LONG result =
		RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey);
	if (result != ERROR_SUCCESS)
	{
		return false;
	}
	DWORD type = 0;
	DWORD size = 0;
	result = RegQueryValueExW(hKey, L"HPR", NULL, &type, NULL, &size);
	RegCloseKey(hKey);
	return (result == ERROR_SUCCESS);
#else
	const char *home = std::getenv("HOME");
	if (!home)
		return false;
	std::string autostartFile = std::string(home) + "/.config/autostart/hpr.desktop";
	return std::filesystem::exists(autostartFile) || std::filesystem::is_symlink(autostartFile);
#endif
}

void AutostartManager::setEnabled(bool enable, bool headless)
{
#ifdef _WIN32
	HKEY hKey;
	LONG result =
		RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey);
	if (result != ERROR_SUCCESS)
	{
		return;
	}
	if (enable)
	{
		wchar_t path[MAX_PATH];
		GetModuleFileNameW(NULL, path, MAX_PATH);
		std::wstring cmd = L"\"" + std::wstring(path) + L"\"";
		// Note: headless-mode is handled via config.csv directly, so no command
		// line argument is appended
		RegSetValueExW(hKey, L"HPR", 0, REG_SZ, (const BYTE *)cmd.c_str(),
					   (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
	}
	else
	{
		RegDeleteValueW(hKey, L"HPR");
	}
	RegCloseKey(hKey);
#else
	const char *home = std::getenv("HOME");
	if (!home)
		return;

	std::string autostartDir = std::string(home) + "/.config/autostart/";
	std::string autostartFile = autostartDir + "hpr.desktop";

	// Delete existing link/file first to avoid symlink overwrite failure
	std::filesystem::remove(autostartFile);

	if (enable)
	{
		std::filesystem::create_directories(autostartDir);

		// Find the pre-existing desktop file created by HPR
		std::string sourceFile = "/usr/share/applications/hpr.desktop";
		if (!std::filesystem::exists(sourceFile))
		{
			sourceFile = std::string(home) + "/.local/share/applications/hpr.desktop";
		}

		if (std::filesystem::exists(sourceFile))
		{
			std::error_code ec;
			std::filesystem::create_symlink(sourceFile, autostartFile, ec);
			if (ec)
			{
				std::cerr << "Failed to create autostart symlink: " << ec.message() << std::endl;
			}
		}
		else
		{
			// Fallback if launcher desktop file is not found (e.g. running from
			// build dir)
			std::ofstream desktop(autostartFile);
			if (desktop.is_open())
			{
				std::string execPath = std::filesystem::canonical("/proc/self/exe").string();
				std::string iconPath = std::string(home) + "/.config/HPR/icons/hicolor/256x256/apps/hpr.png";

				desktop << "[Desktop Entry]\n"
						<< "Version=1.0\n"
						<< "Type=Application\n"
						<< "Name=HPR\n"
						<< "Comment=Offline zero-account activity tracker\n"
						<< "Exec=" << execPath << "\n"
						<< "Icon=" << iconPath << "\n"
						<< "Terminal=false\n"
						<< "Categories=Utility;\n"
						<< "StartupNotify=true\n"
						<< "StartupWMClass=HPR\n";
				desktop.close();
				chmod(autostartFile.c_str(), S_IRWXU | S_IRGRP | S_IROTH);
			}
		}
	}
#endif
}
