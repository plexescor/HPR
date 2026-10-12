#include "databaseManager.hpp"
#include "patternAnalyzer.hpp"
#include "appEvents.hpp"
#include "appState.hpp"
#include "extensionManager.hpp"
#include "logger.hpp"
#include "timeUtils.hpp"
#include <cmath>
#include <sqlite3.h> //for extensinos ot execute sql
#include <sqlite_modern_cpp.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

DatabaseManager::DatabaseManager()
{
	#ifdef NDEBUG
		initDatabase();

		// Try to create a lock file
		std::filesystem::path lockPath = filePath / "hpr.lock";

	#ifdef _WIN32
		lockHandle = CreateFileA(lockPath.string().c_str(), GENERIC_WRITE,
								0, // 0 = exclusive access
								NULL,
								OPEN_ALWAYS, // Create if missing, open if exist, crucial if HPR crashed
								FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
		if (lockHandle == INVALID_HANDLE_VALUE)
		{
			DWORD err = GetLastError();
			if (err == ERROR_SHARING_VIOLATION)
			{
				std::cerr << "[HPR] Already running. Exiting.\n";
				Logger::log("[HPR] Already running. Exiting.");
			}
			else
			{
				std::cerr << "[HPR] Could not acquire lock (Error " << err << ").\n";
				Logger::log("[HPR] Could not acquire lock (Error " + std::to_string(err) + ").");
			}
			exit(1);
		}
	#else
		int fd = open(lockPath.c_str(), O_RDWR | O_CREAT, 0644);
		if (fd == -1)
		{
			std::cerr << "[HPR] Failed to open lock file.\n";
			Logger::log("[HPR] Failed to open lock file.");
			exit(1);
		}
		if (flock(fd, LOCK_EX | LOCK_NB) == -1)
		{
			std::cerr << "[HPR] Already running. Exiting.\n";
			Logger::log("[HPR] Already running. Exiting.");
			close(fd);
			exit(1);
		}
		lockFd = fd;
	#endif
		if (!loadStateFromDB())
		{
			std::cerr << "Failed to load data from db!\n";
			Logger::log("Failed to load data from db!");
		}
	#endif 
	// Connect to the event manager and get an id
	// Listen for load singular db file signal
	singular_DbLoadEventId = EventHub::connect(Event::LOAD_DATABASE_SINGULAR,
											   [this](EventData data)
											   {
												   // If desired data exists
												   if (std::holds_alternative<DatabaseDate_Singular>(data))
												   {
													   std::string requestedDate =
														   std::get<DatabaseDate_Singular>(data).date;
													   this->loadDb_Singular(requestedDate);
												   }
											   });

	number_DbLoadEventId = EventHub::connect(Event::LOAD_DATABASE_NUMBER,
											 [this](EventData data)
											 {
												 if (std::holds_alternative<DatabaseDate_Number>(data))
												 {
													 auto arg = std::get<DatabaseDate_Number>(data);
													 this->loadDb_Number(arg.days, arg.mode, true);
												 }
											 });

	range_DbLoadEventId = EventHub::connect(Event::LOAD_DATABASE_RANGE,
											[this](EventData data)
											{
												if (std::holds_alternative<DatabaseDate_Range>(data))
												{
													auto arg = std::get<DatabaseDate_Range>(data);
													this->loadDb_Range(arg.dateFrom, arg.dateTo, arg.mode, true);
												}
											});

	patterns_DbLoadEventId = EventHub::connect(Event::LOAD_PATTERNS_DATA,
											   [this](EventData data)
											   {
												   if (std::holds_alternative<PatternDataRequest>(data))
												   {
													   int days = std::get<PatternDataRequest>(data).days;
													   this->loadPatternsData(days);
												   }
											   });
}

DatabaseManager::~DatabaseManager()
{
	#ifdef NDEBUG
		running = false;
		if (writer.joinable())
			writer.join();

	#ifdef _WIN32
		if (lockHandle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(lockHandle); // windows autodelete lock file
		}
	#else
		if (lockFd != -1)
		{
			close(lockFd); // kernal will delete the lock file auto
		}
	#endif
	#endif

	EventHub::disconnect(Event::LOAD_DATABASE_SINGULAR, singular_DbLoadEventId);
	EventHub::disconnect(Event::LOAD_DATABASE_NUMBER, number_DbLoadEventId);
	EventHub::disconnect(Event::LOAD_DATABASE_RANGE, range_DbLoadEventId);
	EventHub::disconnect(Event::LOAD_PATTERNS_DATA, patterns_DbLoadEventId);
}

void DatabaseManager::initDatabase(bool copyData)
{

	updateFilePath();
	updateFileName();

	if (copyData)
	{
		// We need copies of the data for extra safety
		{
			std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
			timeLog_PerApp_D = AppState::state.timeLog_PerApp;
			timeLog_PerTab_D = AppState::state.timeLog_PerTab;
			timeLog_PerProject_D = AppState::state.timeLog_PerProject;
			switchHistory_D = AppState::state.switchHistory;
			appLimits_D = AppState::state.appLimits;
			appGoals_D = AppState::state.appGoals;
		}
	}

	db.emplace((filePath / fileName).string());

	// WAL mode + synchronous normal — critical for Btrfs+LUKS
	*db << "PRAGMA journal_mode=WAL;";
	*db << "PRAGMA synchronous=NORMAL;";

	*db << "create table if not exists app_usage ("
		   "   name text unique,"
		   "   duration int"
		   ");";

	*db << "create table if not exists tab_usage("
		   "    name text unique,"
		   "    duration int"
		   ");";

	*db << "create table if not exists project_usage("
		   "    name text unique,"
		   "    duration int"
		   ");";

	*db << "create table if not exists switch_history ("
		   "   fromWindow text,"
		   "   toWindow text,"
		   "   timeStamp int unique"
		   ");";

	*db << "create table if not exists app_limits ("
		   "   name text unique,"
		   "   minutes int"
		   ");";

	*db << "create table if not exists app_goals ("
		   "   name text unique,"
		   "   minutes int"
		   ");";

	*db << "create table if not exists limit_bases ("
		   "   name text unique,"
		   "   base_ms int"
		   ");";
	*db << "create table if not exists goal_bases ("
		   "   name text unique,"
		   "   base_ms int"
		   ");";
	
	// We dont need date here because the db file's name is already the date
	*db << "create table if not exists productivity_scores_daily ("
		   "   score int"
		   ");";

	*db << "create table if not exists productivity_scores_hourly ("
		   "   hour int unique,"
		   "   score int"
		   ");";
}

void DatabaseManager::run() 
{ 
	#ifdef NDEBUG
		writer = std::thread(&DatabaseManager::writeLoop, this); 
	#endif
}

