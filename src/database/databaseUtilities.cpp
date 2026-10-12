#include "extensionManager.hpp"
#include "databaseManager.hpp"
#include "timeUtils.hpp"
#include "appState.hpp"
#include "logger.hpp"

#include <string>
#include <filesystem>

std::string DatabaseManager::getDbPathForDate(const std::string &date)
{
	if (AppState::extManager)
	{
		auto res = AppState::extManager->dispatchOverride("getDbPathForDate", {CppValue(CppValue::Type::String, date)});
		if (res.has_value() && res->type == CppValue::Type::String)
		{
			return res->str_val;
		}
	}
	std::filesystem::path path;
#ifdef _WIN32
	path = std::getenv("APPDATA");
	path /= std::filesystem::path("HPR/HPR_DB/") / extractMMYY_from_DDMMYY(date) / (date + ".db");
#else
	const char *home = std::getenv("HOME");
	if (!home)
		throw std::runtime_error("HOME env var not set");
	path = home;
	path /= std::filesystem::path(".local/share/HPR/HPR_DB") / extractMMYY_from_DDMMYY(date) / (date + ".db");
#endif
	return path.string();
}

std::string DatabaseManager::getLoadedHistDbPath() const
{
	if (AppState::extManager)
	{
		auto res = AppState::extManager->dispatchOverride("getLoadedHistDbPath", {});
		if (res.has_value() && res->type == CppValue::Type::String)
		{
			return res->str_val;
		}
	}
	return loadedHistDbPath.string();
}

void DatabaseManager::updateFilePath()
{
	// Now construct the path acc to the date

	// Get ms sinc epoch
	auto nowSystem = std::chrono::system_clock::now();
	uint64_t t = std::chrono::duration_cast<std::chrono::milliseconds>(nowSystem.time_since_epoch()).count();

	std::filesystem::path tempPath;
#ifdef _WIN32
	tempPath = std::getenv("APPDATA");
	tempPath /= std::filesystem::path("HPR/HPR_DB");
#else
	const char *home = std::getenv("HOME");
	if (!home)
		throw std::runtime_error("HOME env var not set");
	tempPath = home;
	tempPath /= std::filesystem::path(".local/share/HPR/HPR_DB");
#endif

	filePath = tempPath;

	// check if config.csv defines a db path
	// if yes then switch to it, else
	std::filesystem::path customPath = std::filesystem::path(
			AppState::configManager.getConfig<std::string>("database-root-folder-path", filePath.string())
		);

	// We dont want to deal with ~
	if (customPath.is_absolute())
	{
		try
		{
			std::filesystem::create_directories(customPath);
			customPath = std::filesystem::canonical(customPath);
			filePath = customPath;
		}
		catch (const std::filesystem::filesystem_error& e)
		{
			std::cerr << "[Databse Manager] Failed to resolve custom database path: " << e.what() << std::endl;
			Logger::log(std::string("[Databse Manager] Failed to resolve custom database path: ") + e.what() + "\n");
		}
	}
	else
	{
		std::cerr << "[Databse Manager] The custom provided database path is not absolute: " << customPath.string() << std::endl;
		Logger::log(std::string("[Databse Manager] The custom provided database path is not absolute: ") + customPath.string() + "\n");
		std::filesystem::create_directories(filePath);
	}

	filePath /= convertToDate_MMYY(t);
	
	std::filesystem::create_directories(filePath);
}

void DatabaseManager::updateFileName()
{
	std::string tempName = "";
	// Get ms sinc epoch
	auto nowSystem = std::chrono::system_clock::now();
	uint64_t t = std::chrono::duration_cast<std::chrono::milliseconds>(nowSystem.time_since_epoch()).count();

	tempName += convertToDate_DDMMYY(t) + ".db";

	fileName = tempName;
}