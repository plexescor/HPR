#include "linuxUtilities.hpp"
#include "logger.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
// Static member definition lazy bruv
std::string LinuxInitialiser::s_iconThemePath;

const std::string &LinuxInitialiser::getIconThemePath() { return s_iconThemePath; }

LinuxInitialiser::LinuxInitialiser()
{
#ifdef __linux__

	const char *home = std::getenv("HOME");
	if (!home)
		throw std::runtime_error("HOME env var not set");

	std::filesystem::path configDir = std::filesystem::path(home) / ".config/HPR/";
	std::filesystem::path assetsDir = configDir / "assets/";

	std::filesystem::create_directories(assetsDir);
	filePath = assetsDir / iconName;

	std::ifstream file(filePath);
	if (!file.is_open())
	{
		std::cerr << "Warning: " << iconName << " not found at " << filePath.string() << ". App will have no icon.\n";
		Logger::log("Warning: " + iconName + " not found at " + filePath.string() + ". App will have no icon.");
		return;
	}

	// Register the icon into a local hicolor theme directory.
	// This is what TrayManager reads via IconThemePath to show the tray icon.
	std::filesystem::path themeAppsDir = configDir / "icons/hicolor/256x256/apps/";
	std::filesystem::create_directories(themeAppsDir);
	std::filesystem::path themeIconPath = themeAppsDir / "hpr.png";

	if (!std::filesystem::exists(themeIconPath) ||
		std::filesystem::last_write_time(filePath) > std::filesystem::last_write_time(themeIconPath))
	{
		std::filesystem::copy_file(filePath, themeIconPath, std::filesystem::copy_options::overwrite_existing);
	}

	// Write index.theme — required by the XDG icon theme spec so that
	// St.IconTheme (used by GNOME's AppIndicator extension) recognises
	// this directory as a valid icon theme and can resolve "hpr" → hpr.png.
	// Without this file, St.IconTheme::lookup_icon_for_scale() returns null
	// and the tray icon silently shows nothing.  Other DEs are unaffected.
	std::filesystem::path indexThemePath = configDir / "icons/hicolor/index.theme";
	if (!std::filesystem::exists(indexThemePath))
	{
		std::ofstream indexTheme(indexThemePath);
		if (indexTheme.is_open())
		{
			indexTheme << "[Icon Theme]\n"
					   << "Name=hicolor\n"
					   << "Comment=HPR hicolor icon theme\n"
					   << "Directories=256x256/apps\n"
					   << "\n"
					   << "[256x256/apps]\n"
					   << "Size=256\n"
					   << "Type=Fixed\n";
			indexTheme.close();
		}
	}

	s_iconThemePath = configDir / "icons";

	// Write a .desktop file to ~/.local/share/applications/hpr.desktop.
	// This is what the taskbar (both X11 WMs and Wayland compositors) uses
	// to associate the window with its icon.  StartupWMClass=HPR must match
	// the xdg app-id set by slint::set_xdg_app_id("HPR") in HPR.cpp.
	// Skip if a system-wide desktop file already exists (AUR/PKGBUILD install).
	// In that case the system icon + desktop entry are already correct and we
	// never even reach here anyway (assets won't be in ~/.config/HPR/assets/).
	if (std::filesystem::exists("/usr/share/applications/hpr.desktop"))
		return;

	std::filesystem::path desktopDir = std::filesystem::path(home) / ".local/share/applications/";
	std::filesystem::create_directories(desktopDir);
	std::filesystem::path desktopFile = desktopDir / "hpr.desktop";

	bool shouldWrite = true;
	std::string expectedExec = "Exec=" + std::filesystem::canonical("/proc/self/exe").string();
	std::string expectedIcon = "Icon=" + themeIconPath.string();

	if (std::filesystem::exists(desktopFile))
	{
		std::ifstream input(desktopFile);
		std::string line;
		bool execMatches = false;
		bool iconMatches = false;
		while (std::getline(input, line))
		{
			if (line == expectedExec)
				execMatches = true;
			if (line == expectedIcon)
				iconMatches = true;
		}
		if (execMatches && iconMatches)
		{
			shouldWrite = false;
		}
	}

	if (shouldWrite)
	{
		//Only create the desktop entry if its not debug
		#ifdef NDEBUG
			std::ofstream desktop(desktopFile);
			if (desktop.is_open())
			{
				desktop << "[Desktop Entry]\n"
						<< "Version=1.0\n"
						<< "Type=Application\n"
						<< "Name=HPR\n"
						<< "Comment=Offline zero-account activity tracker\n"
						<< expectedExec << "\n"
						<< "Icon=" << themeIconPath.string() << "\n"
						<< "Terminal=false\n"
						<< "Categories=Utility;\n"
						<< "StartupNotify=true\n"
						<< "StartupWMClass=HPR\n";
				desktop.close();

				// Desktop files must be executable to be respected by the DE
				chmod(desktopFile.c_str(), S_IRWXU | S_IRGRP | S_IROTH);
			}
			else
			{
				std::cerr << "Warning: could not write .desktop file to " << desktopFile.string() << "\n";
				Logger::log("Warning: could not write .desktop file to " + desktopFile.string());
			}
		#endif
	}
#endif
}

LinuxInitialiser::~LinuxInitialiser() {}
