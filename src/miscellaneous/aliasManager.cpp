#include "logger.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#ifdef _WIN32
#include <windows.h>
#endif

#include "aliasManager.hpp"
#include "appState.hpp"
#include "extensionManager.hpp"

AliasManager::AliasManager()
{
	loadAliases();
	loadAliases_Tab();
	loadAliases_Project();
}

void AliasManager::loadAliases()
{
	aliasList.clear(); // clear the vector if we ever reload alises.csv
	std::filesystem::path tempPath;
#ifdef _WIN32
	tempPath = std::getenv("APPDATA");
	tempPath /= std::filesystem::path("HPR/HPR_Config/");
#else
	const char *home = std::getenv("HOME");
	if (!home)
		throw std::runtime_error("HOME env var not set");
	tempPath = home;
	tempPath /= std::filesystem::path(".config/HPR/");
#endif

	std::filesystem::create_directories(tempPath);
	filePath = tempPath / fileName;

	std::ifstream file(filePath);

	//Create the aliases.csv file and paste default aliases:
	if (!file.is_open())
	{
		std::ofstream createDefaultFile(filePath);
		if (createDefaultFile.is_open())
		{
			createDefaultFile << R"CSV(# ============================================================================
# HPR Aliases Configuration File
# 
# HOW TO ADD NEW APPS:
# 1. The format is exactly: raw name,Pretty Name (raw name must be fully lowercase 
#    and must be contained in whatever you see in the ui)
# 2. HPR uses "substring matching"! If you add `code,Visual Studio`, it will 
#    automatically catch `code`, `vscode`, `code.exe`, `code-oss`, etc.
# 3. Do NOT put spaces around the comma unless you specifically want them!
# 4. Lines starting with # (like this one) or blank lines are safely ignored.
# 5. Changes are applied INSTANTLY! HPR detects edits automatically.
#    No restart needed - just save the file and watch the UI update.
# ============================================================================

# --- Web Browsers ---
chrome,Chrome
google,Chrome
chromium,Chrome
msedge,Edge
edge,Edge
firefox,Firefox
brave,Brave

# --- Development & Productivity ---
code,Visual Studio Code
vscodium,Visual Studio Code
devenv,Visual Studio
obsidian,Obsidian
postman,Postman
notion,Notion
notepad,Notepad
antigravity,Antigravity


# --- File Managers ---
explorer,Explorer
dolphin,Dolphin
nautilus,Nautilus
thunar,Thunar
nemo,Nemo

# --- Media & Communication ---
obs,OBS Studio
spotify,Spotify
discord,Discord
slack,Slack
teams,Microsoft Teams
zoom,Zoom
vlc,VLC Media Player
mpv,Media Player

# --- Games & System Utilities ---
steam,Steam
btop,BTOP++
texteditor,Text Editor
text-editor,Text Editor
taskmgr,Task Manager
systemmonitor,System Monitor

# --- Terminal Emulators (Unified) ---
terminal,Terminal
cmd,Terminal
powershell,Terminal
pwsh,Terminal
ptyxis,Terminal
konsole,Terminal
tilix,Terminal
alacritty,Terminal
kitty,Terminal
hyper,Terminal
terminator,Terminal
guake,Terminal
tilda,Terminal
wezterm,Terminal

# --- Uncategorizable stuff ---
c:,Explorer
perftune,Intel XTU
debugconsole,Visual Studio Debug Console
)CSV";
			createDefaultFile.close();
			std::cerr << "Aliases file not found. Created default aliases.csv at " << filePath.string() << "\n";
			Logger::log("Aliases file not found. Created default aliases.csv at " + filePath.string());
		}
		else
		{
			std::cerr << "Error: Could not create default Aliases file at " << filePath.string() << "\n";
			Logger::log("Error: Could not create default Aliases file at " + filePath.string());
			return;
		}

		file.open(filePath);
		if (!file.is_open())
			return;
	}

	// store last modified time
	lastModified = std::filesystem::last_write_time(filePath);

	std::string line;

	// One line at a time
	while (std::getline(file, line))
	{
		// if empty or starts with #, continue, write comments with #
		if (line.empty() || line.starts_with("#"))
			continue;

		// find comma pos
		size_t commaPos = line.find(',');

		// If it has a comma then do this
		if (commaPos != std::string::npos)
		{
			aliasList.push_back({line.substr(0, commaPos), line.substr(commaPos + 1)});
		}
	}
}

void AliasManager::loadAliases_Tab()
{
	aliasList_Tab.clear(); // clear the vector if we ever reload
	std::filesystem::path tempPath;
#ifdef _WIN32
	tempPath = std::getenv("APPDATA");
	tempPath /= std::filesystem::path("HPR/HPR_Config/");
#else
	const char *home = std::getenv("HOME");
	if (!home)
		throw std::runtime_error("HOME env var not set");
	tempPath = home;
	tempPath /= std::filesystem::path(".config/HPR/");
#endif

	std::filesystem::create_directories(tempPath);
	filePath_Tab = tempPath / fileName_Tab;

	std::ifstream file(filePath_Tab);

	if (!file.is_open())
	{
		std::ofstream createDefaultFile(filePath_Tab);
		if (createDefaultFile.is_open())
		{
			createDefaultFile << R"CSV(# ============================================================================
# HPR Tab Aliases Configuration File
#
# HOW TO ADD NEW TABS:
# 1. The format is exactly: raw name,Pretty Name (raw name must be fully
#    lowercase and must be contained in the window title HPR sees)
# 2. HPR uses "substring matching"! If you add `youtube,YouTube`, it will
#    catch any window title containing "youtube" regardless of page title.
# 3. Do NOT put spaces around the comma unless you specifically want them!
# 4. Lines starting with # (like this one) or blank lines are safely ignored.
# 5. Changes are applied INSTANTLY! HPR detects edits automatically.
#    No restart needed - just save the file and watch the UI update.
#
# NOTE: Raw names must be fully lowercase. HPR lowercases the window title
#       before matching, so casing in the raw column never matters.
# ============================================================================

# --- Video & Streaming ---
youtube,YouTube
netflix,Netflix
prime video,Amazon Prime Video
disney+,Disney+
hotstar,Hotstar
twitch,Twitch
vimeo,Vimeo
crunchyroll,Crunchyroll

# --- Social Media ---
twitter,Twitter
instagram,Instagram
facebook,Facebook
reddit,Reddit
linkedin,LinkedIn
threads,Threads
pinterest,Pinterest
tumblr,Tumblr

# --- Messaging & Communication ---
whatsapp,WhatsApp Web
gmail,Gmail
outlook,Outlook
telegram,Telegram

# --- Development & Productivity ---
github,GitHub
gitlab,GitLab
stack overflow,Stack Overflow
codepen,CodePen
replit,Replit
google colab,Google Colab
leetcode,LeetCode
hackerrank,HackerRank
codeforces,Codeforces
vercel,Vercel
netlify,Netlify
google docs,Google Docs
google sheets,Google Sheets
google slides,Google Slides
notion,Notion
figma,Figma
canva,Canva
trello,Trello
jira,Jira
linear,Linear

# --- Search & AI ---
google search,Google Search
bing,Bing
duckduckgo,DuckDuckGo
wikipedia,Wikipedia
claude,Claude
chatgpt,ChatGPT
gemini,Gemini

# --- News & Reading ---
medium,Medium
substack,Substack
hacker news,Hacker News
dev.to,Dev.to
hashnode,Hashnode

# --- Shopping ---
amazon,Amazon
flipkart,Flipkart
meesho,Meesho
myntra,Myntra

# --- Learning ---
udemy,Udemy
coursera,Coursera
khan academy,Khan Academy
w3schools,W3Schools
mdn web docs,MDN Web Docs

# --- 🥚 Easter Egg ---
# "Yea mom i am doing some research on human biology 👉️👌"
# Sites
pornhub,Yea mom i am doing some research on human biology 👉️👌
xvideos,Yea mom i am doing some research on human biology 👉️👌
xnxx,Yea mom i am doing some research on human biology 👉️👌
xhamster,Yea mom i am doing some research on human biology 👉️👌
hamster,Yea mom i am doing some research on human biology 👉️👌
redtube,Yea mom i am doing some research on human biology 👉️👌
youporn,Yea mom i am doing some research on human biology 👉️👌
brazzers,Yea mom i am doing some research on human biology 👉️👌
onlyfans,Yea mom i am doing some research on human biology 👉️👌
fapello,Yea mom i am doing some research on human biology 👉️👌
spankbang,Yea mom i am doing some research on human biology 👉️👌
rule34,Yea mom i am doing some research on human biology 👉️👌
e-hentai,Yea mom i am doing some research on human biology 👉️👌
nhentai,Yea mom i am doing some research on human biology 👉️👌
hentaihaven,Yea mom i am doing some research on human biology 👉️👌
naughty america,Yea mom i am doing some research on human biology 👉️👌
bangbros,Yea mom i am doing some research on human biology 👉️👌
realitykings,Yea mom i am doing some research on human biology 👉️👌
mofos,Yea mom i am doing some research on human biology 👉️👌
# Trigger words
xxx,Yea mom i am doing some research on human biology 👉️👌
porn,Yea mom i am doing some research on human biology 👉️👌
sex,Yea mom i am doing some research on human biology 👉️👌
cum,Yea mom i am doing some research on human biology 👉️👌
fucking,Yea mom i am doing some research on human biology 👉️👌
squirt,Yea mom i am doing some research on human biology 👉️👌
hentai,Yea mom i am doing some research on human biology 👉️👌
nude,Yea mom i am doing some research on human biology 👉️👌
nsfw,Yea mom i am doing some research on human biology 👉️👌
blowjob,Yea mom i am doing some research on human biology 👉️👌
milf,Yea mom i am doing some research on human biology 👉️👌
anal,Yea mom i am doing some research on human biology 👉️👌
creampie,Yea mom i am doing some research on human biology 👉️👌
gangbang,Yea mom i am doing some research on human biology 👉️👌
handjob,Yea mom i am doing some research on human biology 👉️👌
# Actors & Actresses
mia khalifa,Yea mom i am doing some research on human biology 👉️👌
riley reid,Yea mom i am doing some research on human biology 👉️👌
lana rhoades,Yea mom i am doing some research on human biology 👉️👌
abella danger,Yea mom i am doing some research on human biology 👉️👌
alexis texas,Yea mom i am doing some research on human biology 👉️👌
jenna jameson,Yea mom i am doing some research on human biology 👉️👌
lisa ann,Yea mom i am doing some research on human biology 👉️👌
brandi love,Yea mom i am doing some research on human biology 👉️👌
kendra lust,Yea mom i am doing some research on human biology 👉️👌
johnny sins,Yea mom i am doing some research on human biology 👉️👌
james deen,Yea mom i am doing some research on human biology 👉️👌
ryan conner,Yea mom i am doing some research on human biology 👉️👌
luna star,Yea mom i am doing some research on human biology 👉️👌
)CSV";
			createDefaultFile.close();
			std::cerr << "TabAliases file not found. Created default tabAliases.csv at " << filePath_Tab.string() << "\n";
			Logger::log("TabAliases file not found. Created default tabAliases.csv at " + filePath_Tab.string());
		}
		else
		{
			std::cerr << "Error: Could not create default TabAliases file at " << filePath_Tab.string() << "\n";
			Logger::log("Error: Could not create default TabAliases file at " + filePath_Tab.string());
			return;
		}

		file.open(filePath_Tab);
		if (!file.is_open())
			return;
	}

	// store last modified time
	lastModified_Tab = std::filesystem::last_write_time(filePath_Tab);

	std::string line;

	// One line at a time
	while (std::getline(file, line))
	{
		// if empty or starts with #, continue, write comments with #
		if (line.empty() || line.starts_with("#"))
			continue;

		// find comma pos
		size_t commaPos = line.find(',');

		// If it has a comma then do this
		if (commaPos != std::string::npos)
		{
			aliasList_Tab.push_back({line.substr(0, commaPos), line.substr(commaPos + 1)});
		}
	}
}

void AliasManager::loadAliases_Project()
{
	aliasList_Project.clear(); // clear the vector if we ever reload
	std::filesystem::path tempPath;
#ifdef _WIN32
	tempPath = std::getenv("APPDATA");
	tempPath /= std::filesystem::path("HPR/HPR_Config/");
#else
	const char *home = std::getenv("HOME");
	if (!home)
		throw std::runtime_error("HOME env var not set");
	tempPath = home;
	tempPath /= std::filesystem::path(".config/HPR/");
#endif

	std::filesystem::create_directories(tempPath);
	filePath_Project = tempPath / fileName_Project;

	std::ifstream file(filePath_Project);

	if (!file.is_open())
	{
		std::ofstream createDefaultFile(filePath_Project);
		if (createDefaultFile.is_open())
		{
			createDefaultFile << R"CSV(# ============================================================================
# HPR VS Code Projects Configuration File
# 
# -Intentionally empty so you can tag your vs code Projects
#   to anything you want for example: fisherPRO to Client A
#   that format:  rawName,What you want
#   make sure the rawName is in all lowercase no matter what
#
# ============================================================================
)CSV";
			createDefaultFile.close();
			std::cerr << "ProjectAliases file not found. Created default projectAliases.csv at " << filePath_Project.string() << "\n";
			Logger::log("ProjectAliases file not found. Created default projectAliases.csv at " + filePath_Project.string());
		}
		else
		{
			std::cerr << "Error: Could not create default ProjectAliases file at " << filePath_Project.string() << "\n";
			Logger::log("Error: Could not create default ProjectAliases file at " + filePath_Project.string());
			return;
		}

		file.open(filePath_Project);
		if (!file.is_open())
			return;
	}

	// store last modified time
	lastModified_Project = std::filesystem::last_write_time(filePath_Project);

	std::string line;

	// One line at a time
	while (std::getline(file, line))
	{
		// if empty or starts with #, continue, write comments with #
		if (line.empty() || line.starts_with("#"))
			continue;

		// find comma pos
		size_t commaPos = line.find(',');

		// If it has a comma then do this
		if (commaPos != std::string::npos)
		{
			aliasList_Project.push_back({line.substr(0, commaPos), line.substr(commaPos + 1)});
		}
	}
}

std::string AliasManager::getAlias(const std::string &rawName)
{
	if (AppState::extManager)
	{
		auto res = AppState::extManager->dispatchOverride("getAlias", {CppValue(CppValue::Type::String, rawName)});
		if (res.has_value() && res->type == CppValue::Type::String)
		{
			return res->str_val;
		}
	}
	std::lock_guard lock(mutex);
	std::string lowerName = rawName;

	// Convert to lower case
	std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
				   [](unsigned char c) { return std::tolower(c); });

	// hot reload check
	if (std::filesystem::exists(filePath))
	{
		auto currentModified = std::filesystem::last_write_time(filePath);
		if (currentModified > lastModified)
		{
			cacheDictionary.clear();
			reverseCacheDictionary.clear();
			loadAliases();
		}
	}

	std::string prettyName;
	auto it = cacheDictionary.find(lowerName);
	if (it != cacheDictionary.end())
	{
		prettyName = it->second;
	}
	else
	{
		bool found = false;
		for (const auto &[triggerWord, pName] : aliasList)
		{
			if (lowerName.contains(triggerWord))
			{
				cacheDictionary[lowerName] = pName;
				prettyName = pName;
				found = true;
				break;
			}
		}
		if (!found)
		{
			cacheDictionary[lowerName] = rawName;
			prettyName = rawName;
		}
	}

	std::string lowerPretty = prettyName;
	std::transform(lowerPretty.begin(), lowerPretty.end(), lowerPretty.begin(),
				   [](unsigned char c) { return std::tolower(c); });
	reverseCacheDictionary[lowerPretty] = rawName;

	return prettyName;
}

std::string AliasManager::getAlias_Tab(const std::string &rawName)
{
	if (AppState::extManager)
	{
		auto res = AppState::extManager->dispatchOverride("getAlias_Tab", {CppValue(CppValue::Type::String, rawName)});
		if (res.has_value() && res->type == CppValue::Type::String)
		{
			return res->str_val;
		}
	}
	std::lock_guard lock(mutex);
	std::string lowerName = rawName;

	// Convert to lower case
	std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
				   [](unsigned char c) { return std::tolower(c); });

	// hot reload check
	if (std::filesystem::exists(filePath_Tab))
	{
		auto currentModified = std::filesystem::last_write_time(filePath_Tab);
		if (currentModified > lastModified_Tab)
		{
			cacheDictionary_Tab.clear();
			reverseCacheDictionary_Tab.clear();
			loadAliases_Tab();
		}
	}

	std::string prettyName;
	auto it = cacheDictionary_Tab.find(lowerName);
	if (it != cacheDictionary_Tab.end())
	{
		prettyName = it->second;
	}
	else
	{
		bool found = false;
		for (const auto &[triggerWord, pName] : aliasList_Tab)
		{
			if (lowerName.contains(triggerWord))
			{
				cacheDictionary_Tab[lowerName] = pName;
				prettyName = pName;
				found = true;
				break;
			}
		}
		if (!found)
		{
			cacheDictionary_Tab[lowerName] = rawName;
			prettyName = rawName;
		}
	}

	std::string lowerPretty = prettyName;
	std::transform(lowerPretty.begin(), lowerPretty.end(), lowerPretty.begin(),
				   [](unsigned char c) { return std::tolower(c); });
	reverseCacheDictionary_Tab[lowerPretty] = rawName;

	return prettyName;
}

std::string AliasManager::getAlias_Project(const std::string &rawName)
{
	if (AppState::extManager)
	{
		auto res =
			AppState::extManager->dispatchOverride("getAlias_Project", {CppValue(CppValue::Type::String, rawName)});
		if (res.has_value() && res->type == CppValue::Type::String)
		{
			return res->str_val;
		}
	}
	std::lock_guard lock(mutex);
	std::string lowerName = rawName;

	// Convert to lower case
	std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
				   [](unsigned char c) { return std::tolower(c); });

	// hot reload check
	if (std::filesystem::exists(filePath_Project))
	{
		auto currentModified = std::filesystem::last_write_time(filePath_Project);
		if (currentModified > lastModified_Project)
		{
			cacheDictionary_Project.clear();
			reverseCacheDictionary_Project.clear();
			loadAliases_Project();
		}
	}

	std::string prettyName;
	auto it = cacheDictionary_Project.find(lowerName);
	if (it != cacheDictionary_Project.end())
	{
		prettyName = it->second;
	}
	else
	{
		bool found = false;
		for (const auto &[triggerWord, pName] : aliasList_Project)
		{
			if (lowerName.contains(triggerWord))
			{
				cacheDictionary_Project[lowerName] = pName;
				prettyName = pName;
				found = true;
				break;
			}
		}
		if (!found)
		{
			cacheDictionary_Project[lowerName] = rawName;
			prettyName = rawName;
		}
	}

	std::string lowerPretty = prettyName;
	std::transform(lowerPretty.begin(), lowerPretty.end(), lowerPretty.begin(),
				   [](unsigned char c) { return std::tolower(c); });
	reverseCacheDictionary_Project[lowerPretty] = rawName;

	return prettyName;
}

std::string AliasManager::getReverseAlias(const std::string &aliasName)
{
	if (AppState::extManager)
	{
		auto res =
			AppState::extManager->dispatchOverride("getReverseAlias", {CppValue(CppValue::Type::String, aliasName)});
		if (res.has_value() && res->type == CppValue::Type::String)
		{
			return res->str_val;
		}
	}
	std::lock_guard lock(mutex);
	std::string lowerName = aliasName;

	// Convert to lower case
	std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
				   [](unsigned char c) { return std::tolower(c); });

	// hot reload check
	if (std::filesystem::exists(filePath))
	{
		auto currentModified = std::filesystem::last_write_time(filePath);
		if (currentModified > lastModified)
		{
			reverseCacheDictionary.clear();
			loadAliases();
		}
	}

	// check cache first
	auto it = reverseCacheDictionary.find(lowerName);

	// return if found in O(1)
	if (it != reverseCacheDictionary.end())
	{
		return it->second;
	}

	for (const auto &[triggerWord, prettyName] : aliasList)
	{
		std::string lowerPretty = prettyName;

		std::transform(lowerPretty.begin(), lowerPretty.end(), lowerPretty.begin(),
					   [](unsigned char c) { return std::tolower(c); });

		if (lowerName.contains(lowerPretty))
		{
			reverseCacheDictionary[lowerName] = triggerWord;
			return triggerWord;
		}
	}

	// if nowhere, then return it as it is and save it in hashmap
	reverseCacheDictionary[lowerName] = aliasName;
	return aliasName;
}

std::string AliasManager::getReverseAlias_Tab(const std::string &aliasName)
{
	if (AppState::extManager)
	{
		auto res = AppState::extManager->dispatchOverride("getReverseAlias_Tab",
														  {CppValue(CppValue::Type::String, aliasName)});
		if (res.has_value() && res->type == CppValue::Type::String)
		{
			return res->str_val;
		}
	}
	std::lock_guard lock(mutex);
	std::string lowerName = aliasName;

	// Convert to lower case
	std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
				   [](unsigned char c) { return std::tolower(c); });

	// hot reload check
	if (std::filesystem::exists(filePath_Tab))
	{
		auto currentModified = std::filesystem::last_write_time(filePath_Tab);
		if (currentModified > lastModified_Tab)
		{
			reverseCacheDictionary_Tab.clear();
			loadAliases_Tab();
		}
	}

	// check cache first
	auto it = reverseCacheDictionary_Tab.find(lowerName);

	// return if found in O(1)
	if (it != reverseCacheDictionary_Tab.end())
	{
		return it->second;
	}

	for (const auto &[triggerWord, prettyName] : aliasList_Tab)
	{
		std::string lowerPretty = prettyName;

		std::transform(lowerPretty.begin(), lowerPretty.end(), lowerPretty.begin(),
					   [](unsigned char c) { return std::tolower(c); });

		if (lowerName.contains(lowerPretty))
		{
			reverseCacheDictionary_Tab[lowerName] = triggerWord;
			return triggerWord;
		}
	}

	// if nowhere, then return it as it is and save it in hashmap
	reverseCacheDictionary_Tab[lowerName] = aliasName;
	return aliasName;
}

std::string AliasManager::getReverseAlias_Project(const std::string &aliasName)
{
	if (AppState::extManager)
	{
		auto res = AppState::extManager->dispatchOverride("getReverseAlias_Project",
														  {CppValue(CppValue::Type::String, aliasName)});
		if (res.has_value() && res->type == CppValue::Type::String)
		{
			return res->str_val;
		}
	}
	std::lock_guard lock(mutex);
	std::string lowerName = aliasName;

	// Convert to lower case
	std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
				   [](unsigned char c) { return std::tolower(c); });

	// hot reload check
	if (std::filesystem::exists(filePath_Project))
	{
		auto currentModified = std::filesystem::last_write_time(filePath_Project);
		if (currentModified > lastModified_Project)
		{
			reverseCacheDictionary_Project.clear();
			loadAliases_Project();
		}
	}

	// check cache first
	auto it = reverseCacheDictionary_Project.find(lowerName);

	// return if found in O(1)
	if (it != reverseCacheDictionary_Project.end())
	{
		return it->second;
	}

	for (const auto &[triggerWord, prettyName] : aliasList_Project)
	{
		std::string lowerPretty = prettyName;

		std::transform(lowerPretty.begin(), lowerPretty.end(), lowerPretty.begin(),
					   [](unsigned char c) { return std::tolower(c); });

		if (lowerName.contains(lowerPretty))
		{
			reverseCacheDictionary_Project[lowerName] = triggerWord;
			return triggerWord;
		}
	}

	// if nowhere, then return it as it is and save it in hashmap
	reverseCacheDictionary_Project[lowerName] = aliasName;
	return aliasName;
}