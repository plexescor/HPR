#include "themeManager.hpp"
#include "appState.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

std::map<std::string, std::string> parseCsv(std::string path);

ThemeManager::ThemeManager()
{
	initialisePath();
	reload();
}

std::string ThemeManager::getPathByName(std::string name) { return availableThemes_Bare[name]; }

std::vector<std::string> ThemeManager::getThemeNames()
{
	std::vector<std::string> names;
	for (const auto &entry : availableThemes_Bare)
	{
		names.push_back(entry.first);
	}
	return names;
}

std::string ThemeManager::getCurrentThemeName()
{
	std::string activeTheme = AppState::configManager.getConfig("custom-theme", std::string("Default"));
	return activeTheme;
}

void ThemeManager::reload()
{
	availableThemes.clear();
	availableThemes_Bare.clear();
	availableThemes_Author.clear();
	themePreview.clear();

	availableThemes_Author["Default"] = "Plexescor";

	for (auto const &entry : std::filesystem::directory_iterator{themeDirectory})
	{
		if (entry.is_directory() && std::filesystem::exists(entry.path() / "metadata.csv"))
		{
			std::map<std::string, std::string> content;
			content = parseCsv(entry.path().string() + "/metadata.csv");
			if (content.contains("name") && content.contains("version"))
			{
				std::string themeName = content["name"];
				std::string authorName = content.contains("author") ? content["author"] : "Unknown"; //get yeeted
				std::string themeVersion = content["version"];

				if (themeVersion.starts_with("v")) themeVersion.erase(0, 1);

				availableThemes[{themeName, themeVersion}] = entry.path().string() + "/app-window.slint";
				availableThemes_Author[themeName] = authorName;
				availableThemes_Bare[themeName] = entry.path().string() + "/app-window.slint";

				std::vector<std::string> previews;
				for (int i = 1; i <= 9; ++i)
				{
					std::filesystem::path imgPath = entry.path() / (std::to_string(i) + ".png");
					if (std::filesystem::exists(imgPath))
					{
						previews.push_back(imgPath.string());
					}
					else
					{
						break;
					}
				}
				themePreview[themeName] = previews;
			}
		}
	}
	areThemesAvailable = !availableThemes.empty();
}

std::map<std::string, std::string> parseCsv(std::string path)
{
	std::ifstream file(path);

	std::string line;

	std::map<std::string, std::string> keyValuePair;
	// One line at a time
	while (std::getline(file, line))
	{
		// if empty or starts with #, continue, write comments with #
		if (line.empty() || line[0] == '#')
			continue;

		// find comma pos
		size_t commaPos = line.find(',');

		// If it has a comma then do this
		if (commaPos != std::string::npos)
		{
			std::string key = line.substr(0, commaPos);
			std::string val = line.substr(commaPos + 1);

			// Clean trailing and leading spaces/carriage returns
			auto trim = [](std::string &s)
			{
				while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
				{
					s.pop_back();
				}
				size_t start = s.find_first_not_of(" \t\r\n");
				if (start != std::string::npos)
				{
					s = s.substr(start);
				}
			};
			trim(key);
			trim(val);

			keyValuePair[key] = val;
		}
	}

	return keyValuePair;
}

void ThemeManager::initialisePath()
{
	std::filesystem::path tempPath;
#ifdef _WIN32
	tempPath = std::getenv("APPDATA");
	tempPath /= std::filesystem::path("HPR/HPR_Config/themes/");
#else
	const char *home = std::getenv("HOME");
	if (!home)
		throw std::runtime_error("HOME env var not set");
	tempPath = home;
	tempPath /= std::filesystem::path(".config/HPR/themes/");
#endif

	themeDirectory = tempPath;
	std::filesystem::create_directories(themeDirectory);
}