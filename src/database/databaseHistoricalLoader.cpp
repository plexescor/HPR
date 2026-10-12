#include "databaseManager.hpp"
#include "appEvents.hpp"
#include "timeUtils.hpp"
#include "appState.hpp"
#include "logger.hpp"

#include <string>
#include <filesystem>
#include <iostream>
#include <vector>

void DatabaseManager::loadDb_Singular(std::string requestedDate, bool showToUi)
{
	// Reset the ready flag BEFORE launching async so waiters block correctly
	{
		std::lock_guard<std::mutex> lk(AppState::historyLoadedMutex);
		AppState::historicalData_State.isLoaded = false;
	}

	// Create async task to load the singular db file
	historyLoadTask_Singular =
		std::async(std::launch::async,
				   [this, requestedDate, showToUi]()
				   {
					   try
					   {
						   // this comment is outdated but i will keep it to show
						   // how evil i was

						   // Get the filepath for desired files
						   // DEVELOPER NOTES:
						   // As its the free version, i therefore only allow loading of
						   // the db files created this month of course you can modify the
						   // source code and give everyone the version that handles the
						   // month also and i will not do anything, :)

						   std::filesystem::path path = getDbPathForDate(requestedDate);
						   loadedHistDbPath = path;

						   // Check if shit even exists
						   if (!std::filesystem::exists(path))
						   {
							   EventHub::emit(Event::APP_ERROR, ErrorGui{"File not found! " + path.string()});
							   std::cerr << "Historical file not found: " << path << std::endl;
							   Logger::log("[HPR] Historical file not found: " + path.string());
							   return;
						   }

						   // Load a new db
						   sqlite::database histDb(path.string());

						   // Force so incomplete .db files possibly due to a crash are
						   // fully resolved by a WAL checkpoint
						   histDb << "PRAGMA wal_checkpoint(TRUNCATE);";

						   // Create an intermediate map
						   std::map<std::string, uint64_t> results;
						   std::map<std::string, uint64_t> results_Tab;
						   std::map<std::string, uint64_t> results_Project;
						   std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> results_Switch;

						   // load from db to the map
						   histDb << "select name, duration from app_usage;" >>
							   [&results](std::string name, uint64_t duration) { results[name] = duration; };

						   histDb << "select name, duration from tab_usage;" >>
							   [&results_Tab](std::string name, uint64_t duration) { results_Tab[name] = duration; };

						   histDb << "select name, duration from project_usage;" >>
							   [&results_Project](std::string name, uint64_t duration)
						   { results_Project[name] = duration; };

						   histDb << "select fromWindow, toWindow, timeStamp from "
									 "switch_history;" >>
							   [&results_Switch](std::string from, std::string to, long long ts)
						   { results_Switch[{from, to}].push_back((uint64_t)ts); };

						   // YOU: see appstate for this
						   {
							   std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
							   AppState::historicalData_State.date = requestedDate;
							   AppState::historicalData_State.timeLog_PerApp = results;
							   AppState::historicalData_State.timeLog_PerTab = results_Tab;
							   AppState::historicalData_State.timeLog_PerProject = results_Project;
							   AppState::historicalData_State.switchHistory = results_Switch;
							   AppState::historicalData_State.isLoaded = true;
						   }
						   // Wake up any thread blocked inside dbQueryHistorical_E
						   AppState::historyLoadedCV.notify_all();

						   if (showToUi)
						       EventHub::emit(Event::HISTORY_LOADED_SINGULAR);
					   }
					   catch (const std::exception &e)
					   {
						   std::cerr << "Failed to load history: " << e.what() << std::endl;
						   Logger::log("[HPR] Failed to load history: " + std::string(e.what()));
					   }
				   });
}

void DatabaseManager::loadDb_Number(int days, std::string mode, bool showToUi)
{
	// Reset the ready flag BEFORE launching async so waiters block correctly
	{
		std::lock_guard<std::mutex> lk(AppState::historyLoadedMutex);
		AppState::historicalData_Full_State.isLoaded = false;
	}

	historyLoadTask_Number = std::async(
		std::launch::async,
		[this, days, mode, showToUi]()
		{
			// total mode
			// get todays time stamp
			uint64_t timeStampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
									   std::chrono::system_clock::now().time_since_epoch())
									   .count();

			std::string activeFileName = this->fileName;

			if (mode == "total")
			{
				std::filesystem::path path;
#ifdef _WIN32
				path = std::getenv("APPDATA");
				path /= std::filesystem::path("HPR/HPR_DB/");
#else
				const char *home = std::getenv("HOME");
				if (!home)
					throw std::runtime_error("HOME env var not set");
				path = home;
				path /= std::filesystem::path(".local/share/HPR/HPR_DB/");
#endif

				// the file names to load
				std::vector<std::string> fileNames;
				fileNames.reserve(days);

				std::vector<std::thread> workers;
				workers.reserve(days);

				{
					std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
					AppState::historicalData_Full_State.timeLog_PerApp.clear();
					AppState::historicalData_Full_State.timeLog_PerTab.clear();
					AppState::historicalData_Full_State.timeLog_PerProject.clear();
					AppState::historicalData_Full_State.switchHistory.clear();
				}

				for (int i = 0; i < days; ++i)
				{
					std::string dateStr = convertToDate_DDMMYY(timeStampMs);
					fileNames.push_back(dateStr);
					timeStampMs = (timeStampMs >= 86400000) ? (timeStampMs - 86400000) : 0;
				}

				for (int i = 0; i < days; ++i)
				{
					workers.emplace_back(
						[this, i, &fileNames, &path, activeFileName]()
						{
							try
							{
								// Create intermediate maps
								std::map<std::string, uint64_t> results;
								std::map<std::string, uint64_t> results_Tab;
								std::map<std::string, uint64_t> results_Project;
								std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> results_Switch;

								if (fileNames[i] + ".db" == activeFileName)
								{
									std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
									results = AppState::state.timeLog_PerApp;
									results_Tab = AppState::state.timeLog_PerTab;
									results_Project = AppState::state.timeLog_PerProject;
									results_Switch = AppState::state.switchHistory;
								}
								else
								{
									std::filesystem::path fullPath =
										path / extractMMYY_from_DDMMYY(fileNames[i]) / (fileNames[i] + ".db");

									if (!std::filesystem::exists(fullPath))
									{
										std::cerr << "Historical file not found: " << fullPath << std::endl;
										Logger::log("[HPR] Historical file not "
													"found: " +
													fullPath.string());
										return;
									}

									sqlite::database numDb(fullPath.string());

									// load from db to the map
									numDb << "select name, duration from "
											 "app_usage;" >>
										[&results](std::string name, uint64_t duration) { results[name] = duration; };

									numDb << "select name, duration from "
											 "tab_usage;" >>
										[&results_Tab](std::string name, uint64_t duration)
									{ results_Tab[name] = duration; };

									numDb << "select name, duration from "
											 "project_usage;" >>
										[&results_Project](std::string name, uint64_t duration)
									{ results_Project[name] = duration; };

									numDb << "select fromWindow, toWindow, "
											 "timeStamp from "
											 "switch_history;" >>
										[&results_Switch](std::string from, std::string to, long long ts)
									{ results_Switch[{from, to}].push_back((uint64_t)ts); };
								}

								{
									std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
									for (const auto &[key, value] : results)
									{
										AppState::historicalData_Full_State.timeLog_PerApp[key] += value;
									}
									for (const auto &[key, value] : results_Tab)
									{
										AppState::historicalData_Full_State.timeLog_PerTab[key] += value;
									}
									for (const auto &[key, value] : results_Project)
									{
										AppState::historicalData_Full_State.timeLog_PerProject[key] += value;
									}
									for (const auto &[key, value] : results_Switch)
									{
										AppState::historicalData_Full_State.switchHistory[key].insert(
											AppState::historicalData_Full_State.switchHistory[key].end(), value.begin(),
											value.end());
									}
									AppState::historicalData_Full_State.isLoaded = true;
								}
							}
							catch (const std::exception &e)
							{
								std::cerr << "Failed to load history: " << e.what() << std::endl;
								Logger::log("[HPR] Failed to load history: " + std::string(e.what()));
							}
						});
				}

				for (auto &w : workers)
				{
					if (w.joinable())
						w.join();
				}

			}

			if (mode == "average")
			{
				std::filesystem::path path;
#ifdef _WIN32
				path = std::getenv("APPDATA");
				path /= std::filesystem::path("HPR/HPR_DB/");
#else
				const char *home = std::getenv("HOME");
				if (!home)
					throw std::runtime_error("HOME env var not set");
				path = home;
				path /= std::filesystem::path(".local/share/HPR/HPR_DB/");
#endif

				std::vector<std::string> fileNames;
				fileNames.reserve(days);

				for (int i = 0; i < days; ++i)
				{
					fileNames.push_back(convertToDate_DDMMYY(timeStampMs));
					timeStampMs = (timeStampMs >= 86400000) ? (timeStampMs - 86400000) : 0;
				}

				std::vector<std::map<std::string, uint64_t>> allApp(days);
				std::vector<std::map<std::string, uint64_t>> allTab(days);
				std::vector<std::map<std::string, uint64_t>> allProject(days);
				std::vector<std::map<std::pair<std::string, std::string>, std::vector<uint64_t>>> allSwitches(days);
				std::vector<bool> loaded(days, false);

				std::vector<std::thread> workers;
				workers.reserve(days);

				{
					std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
					AppState::historicalData_Full_State.timeLog_PerApp.clear();
					AppState::historicalData_Full_State.timeLog_PerTab.clear();
					AppState::historicalData_Full_State.timeLog_PerProject.clear();
					AppState::historicalData_Full_State.switchHistory.clear();
					AppState::historicalData_Full_State.isLoaded = false;
				}

				for (int i = 0; i < days; ++i)
				{
					workers.emplace_back(
						[i, &fileNames, &path, &allApp, &allTab, &allProject, &allSwitches, &loaded, activeFileName]()
						{
							try
							{
								if (fileNames[i] + ".db" == activeFileName)
								{
									std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
									allApp[i] = AppState::state.timeLog_PerApp;
									allTab[i] = AppState::state.timeLog_PerTab;
									allProject[i] = AppState::state.timeLog_PerProject;
									allSwitches[i] = AppState::state.switchHistory;
									loaded[i] = true;
								}
								else
								{
									std::filesystem::path fullPath =
										path / extractMMYY_from_DDMMYY(fileNames[i]) / (fileNames[i] + ".db");

									if (!std::filesystem::exists(fullPath))
									{
										std::cerr << "Historical file not found: " << fullPath << std::endl;
										Logger::log("[HPR] Historical file not "
													"found: " +
													fullPath.string());
										return;
									}

									sqlite::database numDb(fullPath.string());

									numDb << "select name, duration from "
											 "app_usage;" >>
										[&, i](std::string name, uint64_t duration) { allApp[i][name] = duration; };

									numDb << "select name, duration from "
											 "tab_usage;" >>
										[&, i](std::string name, uint64_t duration) { allTab[i][name] = duration; };

									numDb << "select name, duration from "
											 "project_usage;" >>
										[&, i](std::string name, uint64_t duration) { allProject[i][name] = duration; };

									numDb << "select fromWindow, toWindow, "
											 "timeStamp from "
											 "switch_history;" >>
										[&, i](std::string from, std::string to, long long ts)
									{ allSwitches[i][{from, to}].push_back((uint64_t)ts); };

									loaded[i] = true;
								}
							}
							catch (const std::exception &e)
							{
								std::cerr << "Failed to load history: " << e.what() << std::endl;
								Logger::log("[HPR] Failed to load history: " + std::string(e.what()));
							}
						});
				}

				for (auto &w : workers)
					if (w.joinable())
						w.join();

				// O(n) average pass
				std::map<std::string, uint64_t> totalApp;
				std::map<std::string, uint64_t> totalTab;
				std::map<std::string, uint64_t> totalProject;
				std::map<std::string, int> countApp;
				std::map<std::string, int> countTab;
				std::map<std::string, int> countProject;
				std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> totalSwitches;
				std::map<std::pair<std::string, std::string>, int> countSwitches;

				for (int i = 0; i < days; ++i)
				{
					if (!loaded[i])
						continue;
					for (const auto &[k, v] : allApp[i])
					{
						totalApp[k] += v;
						countApp[k]++;
					}
					for (const auto &[k, v] : allTab[i])
					{
						totalTab[k] += v;
						countTab[k]++;
					}
					for (const auto &[k, v] : allProject[i])
					{
						totalProject[k] += v;
						countProject[k]++;
					}
					for (const auto &[k, v] : allSwitches[i])
					{
						totalSwitches[k].insert(totalSwitches[k].end(), v.begin(), v.end());
						countSwitches[k]++;
					}
				}

				{
					std::lock_guard<std::mutex> lock(AppState::historyStateMutex);

					for (const auto &[k, v] : totalApp)
						AppState::historicalData_Full_State.timeLog_PerApp[k] = v / countApp[k];

					for (const auto &[k, v] : totalTab)
						AppState::historicalData_Full_State.timeLog_PerTab[k] = v / countTab[k];

					for (const auto &[k, v] : totalProject)
						AppState::historicalData_Full_State.timeLog_PerProject[k] = v / countProject[k];

					for (const auto &[k, v] : totalSwitches)
						AppState::historicalData_Full_State.switchHistory[k] = v;

					AppState::historicalData_Full_State.isLoaded = true;
				}

			}
			if (showToUi)
				EventHub::emit(Event::HISTORY_LOADED_NUMBER);
		});
}

void DatabaseManager::loadDb_Range(std::string dateFrom, std::string dateTo, std::string mode, bool showToUi)
{
	// Reset the ready flag BEFORE launching async so waiters block correctly
	{
		std::lock_guard<std::mutex> lk(AppState::historyLoadedMutex);
		AppState::historicalData_Full_State.isLoaded = false;
	}

	historyLoadTask_Range = std::async(
		std::launch::async,
		[this, dateFrom, dateTo, mode, showToUi]()
		{
			int days = 0;
			// get time stamp of current date
			// context "dateFrom" the date far-er from present
			uint64_t timeStampFrom = parseDate_DDMMYY(dateFrom);
			uint64_t timeStampTo = parseDate_DDMMYY(dateTo);

			if (timeStampFrom == 0 || timeStampTo == 0)
			{
				std::cerr << "Invalid date format! Dates should be in DDMMYY format." << std::endl;
				Logger::log("[HPR] Invalid date format! Dates should be in "
							"DDMMYY format.");
				return;
			}

			// if the user reversed the order of dates
			if (timeStampFrom > timeStampTo)
			{
				timeStampFrom = parseDate_DDMMYY(dateTo);
				timeStampTo = parseDate_DDMMYY(dateFrom);
			}

			// finding total number of days between the two dates
			days = (timeStampTo - timeStampFrom) / 86400000 + 1; // +1 to include both start and end date

			std::string activeFileName = this->fileName;

			if (mode == "total")
			{
				std::filesystem::path path;
#ifdef _WIN32
				path = std::getenv("APPDATA");
				path /= std::filesystem::path("HPR/HPR_DB/");
#else
				const char *home = std::getenv("HOME");
				if (!home)
					throw std::runtime_error("HOME env var not set");
				path = home;
				path /= std::filesystem::path(".local/share/HPR/HPR_DB/");
#endif

				// the file names to load
				std::vector<std::string> fileNames;
				fileNames.reserve(days);

				std::vector<std::thread> workers;
				workers.reserve(days);

				{
					std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
					AppState::historicalData_Full_State.timeLog_PerApp.clear();
					AppState::historicalData_Full_State.timeLog_PerTab.clear();
					AppState::historicalData_Full_State.timeLog_PerProject.clear();
					AppState::historicalData_Full_State.switchHistory.clear();
				}

				for (int i = 0; i < days; ++i)
				{
					std::string dateStr = convertToDate_DDMMYY(timeStampTo);
					fileNames.push_back(dateStr);
					timeStampTo = (timeStampTo >= 86400000) ? (timeStampTo - 86400000) : 0;
				}

				for (int i = 0; i < days; ++i)
				{
					workers.emplace_back(
						[this, i, &fileNames, &path, activeFileName]()
						{
							try
							{
								// Create intermediate maps
								std::map<std::string, uint64_t> results;
								std::map<std::string, uint64_t> results_Tab;
								std::map<std::string, uint64_t> results_Project;
								std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> results_Switch;

								if (fileNames[i] + ".db" == activeFileName)
								{
									std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
									results = AppState::state.timeLog_PerApp;
									results_Tab = AppState::state.timeLog_PerTab;
									results_Project = AppState::state.timeLog_PerProject;
									results_Switch = AppState::state.switchHistory;
								}
								else
								{
									std::filesystem::path fullPath =
										path / extractMMYY_from_DDMMYY(fileNames[i]) / (fileNames[i] + ".db");

									if (!std::filesystem::exists(fullPath))
									{
										std::cerr << "Historical file not found: " << fullPath << std::endl;
										Logger::log("[HPR] Historical file not "
													"found: " +
													fullPath.string());
										return;
									}

									sqlite::database numDb(fullPath.string());

									// load from db to the map
									numDb << "select name, duration from "
											 "app_usage;" >>
										[&results](std::string name, uint64_t duration) { results[name] = duration; };

									numDb << "select name, duration from "
											 "tab_usage;" >>
										[&results_Tab](std::string name, uint64_t duration)
									{ results_Tab[name] = duration; };

									numDb << "select name, duration from "
											 "project_usage;" >>
										[&results_Project](std::string name, uint64_t duration)
									{ results_Project[name] = duration; };

									numDb << "select fromWindow, toWindow, "
											 "timeStamp from "
											 "switch_history;" >>
										[&results_Switch](std::string from, std::string to, long long ts)
									{ results_Switch[{from, to}].push_back((uint64_t)ts); };
								}

								{
									std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
									for (const auto &[key, value] : results)
									{
										AppState::historicalData_Full_State.timeLog_PerApp[key] += value;
									}
									for (const auto &[key, value] : results_Tab)
									{
										AppState::historicalData_Full_State.timeLog_PerTab[key] += value;
									}
									for (const auto &[key, value] : results_Project)
									{
										AppState::historicalData_Full_State.timeLog_PerProject[key] += value;
									}
									for (const auto &[key, value] : results_Switch)
									{
										AppState::historicalData_Full_State.switchHistory[key].insert(
											AppState::historicalData_Full_State.switchHistory[key].end(), value.begin(),
											value.end());
									}
									AppState::historicalData_Full_State.isLoaded = true;
								}
							}
							catch (const std::exception &e)
							{
								std::cerr << "Failed to load history: " << e.what() << std::endl;
								Logger::log("[HPR] Failed to load history: " + std::string(e.what()));
							}
						});
				}

				for (auto &w : workers)
				{
					if (w.joinable())
						w.join();
				}
			}

			if (mode == "average")
			{
				std::filesystem::path path;
#ifdef _WIN32
				path = std::getenv("APPDATA");
				path /= std::filesystem::path("HPR/HPR_DB/");
#else
				const char *home = std::getenv("HOME");
				if (!home)
					throw std::runtime_error("HOME env var not set");
				path = home;
				path /= std::filesystem::path(".local/share/HPR/HPR_DB/");
#endif

				std::vector<std::string> fileNames;
				fileNames.reserve(days);

				for (int i = 0; i < days; ++i)
				{
					fileNames.push_back(convertToDate_DDMMYY(timeStampTo));
					timeStampTo = (timeStampTo >= 86400000) ? (timeStampTo - 86400000) : 0;
				}

				std::vector<std::map<std::string, uint64_t>> allApp(days);
				std::vector<std::map<std::string, uint64_t>> allTab(days);
				std::vector<std::map<std::string, uint64_t>> allProject(days);
				std::vector<std::map<std::pair<std::string, std::string>, std::vector<uint64_t>>> allSwitches(days);
				std::vector<bool> loaded(days, false);

				std::vector<std::thread> workers;
				workers.reserve(days);

				{
					std::lock_guard<std::mutex> lock(AppState::historyStateMutex);
					AppState::historicalData_Full_State.timeLog_PerApp.clear();
					AppState::historicalData_Full_State.timeLog_PerTab.clear();
					AppState::historicalData_Full_State.timeLog_PerProject.clear();
					AppState::historicalData_Full_State.switchHistory.clear();
					AppState::historicalData_Full_State.isLoaded = false;
				}

				for (int i = 0; i < days; ++i)
				{
					workers.emplace_back(
						[i, &fileNames, &path, &allApp, &allTab, &allProject, &allSwitches, &loaded, activeFileName]()
						{
							try
							{
								if (fileNames[i] + ".db" == activeFileName)
								{
									std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
									allApp[i] = AppState::state.timeLog_PerApp;
									allTab[i] = AppState::state.timeLog_PerTab;
									allProject[i] = AppState::state.timeLog_PerProject;
									allSwitches[i] = AppState::state.switchHistory;
									loaded[i] = true;
								}
								else
								{
									std::filesystem::path fullPath =
										path / extractMMYY_from_DDMMYY(fileNames[i]) / (fileNames[i] + ".db");

									if (!std::filesystem::exists(fullPath))
									{
										std::cerr << "Historical file not found: " << fullPath << std::endl;
										Logger::log("[HPR] Historical file not "
													"found: " +
													fullPath.string());
										return;
									}

									sqlite::database numDb(fullPath.string());

									numDb << "select name, duration from "
											 "app_usage;" >>
										[&, i](std::string name, uint64_t duration) { allApp[i][name] = duration; };

									numDb << "select name, duration from "
											 "tab_usage;" >>
										[&, i](std::string name, uint64_t duration) { allTab[i][name] = duration; };

									numDb << "select name, duration from "
											 "project_usage;" >>
										[&, i](std::string name, uint64_t duration) { allProject[i][name] = duration; };

									numDb << "select fromWindow, toWindow, "
											 "timeStamp from "
											 "switch_history;" >>
										[&, i](std::string from, std::string to, long long ts)
									{ allSwitches[i][{from, to}].push_back((uint64_t)ts); };

									loaded[i] = true;
								}
							}
							catch (const std::exception &e)
							{
								std::cerr << "Failed to load history: " << e.what() << std::endl;
								Logger::log("[HPR] Failed to load history: " + std::string(e.what()));
							}
						});
				}

				for (auto &w : workers)
					if (w.joinable())
						w.join();

				// O(n) average pass
				std::map<std::string, uint64_t> totalApp;
				std::map<std::string, uint64_t> totalTab;
				std::map<std::string, uint64_t> totalProject;
				std::map<std::string, int> countApp;
				std::map<std::string, int> countTab;
				std::map<std::string, int> countProject;
				std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> totalSwitches;
				std::map<std::pair<std::string, std::string>, int> countSwitches;

				for (int i = 0; i < days; ++i)
				{
					if (!loaded[i])
						continue;
					for (const auto &[k, v] : allApp[i])
					{
						totalApp[k] += v;
						countApp[k]++;
					}
					for (const auto &[k, v] : allTab[i])
					{
						totalTab[k] += v;
						countTab[k]++;
					}
					for (const auto &[k, v] : allProject[i])
					{
						totalProject[k] += v;
						countProject[k]++;
					}
					for (const auto &[k, v] : allSwitches[i])
					{
						totalSwitches[k].insert(totalSwitches[k].end(), v.begin(), v.end());
						countSwitches[k]++;
					}
				}

				{
					std::lock_guard<std::mutex> lock(AppState::historyStateMutex);

					for (const auto &[k, v] : totalApp)
						AppState::historicalData_Full_State.timeLog_PerApp[k] = v / countApp[k];

					for (const auto &[k, v] : totalTab)
						AppState::historicalData_Full_State.timeLog_PerTab[k] = v / countTab[k];

					for (const auto &[k, v] : totalProject)
						AppState::historicalData_Full_State.timeLog_PerProject[k] = v / countProject[k];

					for (const auto &[k, v] : totalSwitches)
						AppState::historicalData_Full_State.switchHistory[k] = v;

					AppState::historicalData_Full_State.isLoaded = true;
				}

			}
			if (showToUi)
				EventHub::emit(Event::HISTORY_LOADED_RANGE);
		});
}

// Synchronous: loads N days of per-day data into PatternAnalyzer for advanced
// insight computation. Called directly on the EventHub thread 
void DatabaseManager::loadPatternsData(int days)
{
	std::string activeFileName = this->fileName;

	std::filesystem::path basePath;
#ifdef _WIN32
	basePath = std::getenv("APPDATA");
	basePath /= std::filesystem::path("HPR/HPR_DB/");
#else
	const char *home = std::getenv("HOME");
	if (!home)
	{
		std::cerr << "[loadPatternsData] HOME env var not set\n";
		Logger::log("[loadPatternsData] HOME env var not set");
		return;
	}
	basePath = home;
	basePath /= std::filesystem::path(".local/share/HPR/HPR_DB/");
#endif

	// Walk backwards from today, one day at a time
	uint64_t tsMs =
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
			.count();

	std::vector<DayData> result;
	result.reserve(days);

	for (int i = 0; i < days; ++i)
	{
		std::string dateStr = convertToDate_DDMMYY(tsMs);
		tsMs = (tsMs >= 86400000ULL) ? (tsMs - 86400000ULL) : 0;

		DayData dayData;
		dayData.date = dateStr;

		if (dateStr + ".db" == activeFileName)
		{
			// Today's live data — read from AppState under lock
			std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
			dayData.timePerApp = AppState::state.timeLog_PerApp;
			dayData.switchHistory = AppState::state.switchHistory;
		}
		else
		{
			std::filesystem::path fullPath = basePath / extractMMYY_from_DDMMYY(dateStr) / (dateStr + ".db");
			if (!std::filesystem::exists(fullPath))
			{
				// Missing day — skip silently (user may not have data for every
				// day)
				continue;
			}

			try
			{
				sqlite::database dayDb(fullPath.string());
				dayDb << "PRAGMA wal_checkpoint(PASSIVE);";

				dayDb << "select name, duration from app_usage;" >> [&dayData](std::string name, uint64_t duration)
				{ dayData.timePerApp[name] = duration; };

				dayDb << "select fromWindow, toWindow, timeStamp from "
						 "switch_history;" >>
					[&dayData](std::string from, std::string to, long long ts)
				{ dayData.switchHistory[{from, to}].push_back((uint64_t)ts); };
			}
			catch (const std::exception &e)
			{
				std::cerr << "[loadPatternsData] Failed to load " << fullPath << ": " << e.what() << "\n";
				Logger::log("[loadPatternsData] Failed to load " + fullPath.string() + ": " + std::string(e.what()));
				continue;
			}
		}

		result.push_back(std::move(dayData));
	}

	{
		std::lock_guard<std::mutex> lock(AppState::patternAnalyzerMutex);
		AppState::patternAnalyzer.setMultiDayData(std::move(result));
		AppState::patternAnalyzer.generateAdvancedInsights();
	}
}