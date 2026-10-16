#include "databaseManager.hpp"
#include "timeUtils.hpp"
#include "appEvents.hpp"
#include "appState.hpp"
#include "logger.hpp"
#include <chrono>
#include <thread>
#include <string>
#include <mutex>

void DatabaseManager::writeLoop()
{
	while (running)
	{
		bool needsRollover = false;
		std::string newName = "";

		{
			std::lock_guard<std::mutex> dbLock(dbQueryMutex);
			*db << "BEGIN;";

			{
				std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
				timeLog_PerApp_D = AppState::state.timeLog_PerApp;
				timeLog_PerTab_D = AppState::state.timeLog_PerTab;
				timeLog_PerProject_D = AppState::state.timeLog_PerProject;
				switchHistory_D = AppState::state.switchHistory;
				appLimits_D = AppState::state.appLimits;
				appGoals_D = AppState::state.appGoals;
				limitTimeBase_D = AppState::state.limitTimeBase;
				goalTimeBase_D = AppState::state.goalTimeBase;
			}

			for (const auto &[k, v] : timeLog_PerApp_D)
				*db << "insert or replace into app_usage (name,duration) "
					   "values (?,?);"
					<< k << v;

			for (const auto &[k, v] : timeLog_PerTab_D)
				*db << "insert or replace into tab_usage (name,duration) "
					   "values (?,?);"
					<< k << v;

			for (const auto &[k, v] : timeLog_PerProject_D)
				*db << "insert or replace into project_usage (name,duration) "
					   "values "
					   "(?,?);"
					<< k << v;

			for (const auto &[k, v] : switchHistory_D)
			{
				const auto &[from, to] = k;
				for (const auto &timestamp : v)
					*db << "insert or ignore into switch_history "
						   "(fromWindow,toWindow,timeStamp) values (?,?,?);"
						<< from << to << static_cast<long long>(timestamp);
			}

			*db << "delete from app_limits;";
			for (const auto &[k, v] : appLimits_D)
				if (v > 0)
					*db << "insert or replace into app_limits (name,minutes) "
						   "values "
						   "(?,?);"
						<< k << v;

			*db << "delete from app_goals;";
			for (const auto &[k, v] : appGoals_D)
				if (v > 0)
					*db << "insert or replace into app_goals (name,minutes) "
						   "values (?,?);"
						<< k << v;

			*db << "delete from limit_bases;";
			for (const auto &[k, v] : limitTimeBase_D)
				*db << "insert or replace into limit_bases (name,base_ms) "
					   "values (?,?);"
					<< k << v;

			*db << "delete from goal_bases;";
			for (const auto &[k, v] : goalTimeBase_D)
				*db << "insert or replace into goal_bases (name,base_ms) "
					   "values (?,?);"
					<< k << v;

			// We only wanna store the productivity score if its live view
			// as our producitivity calculators show no distiction in result
			// wether the scores are historical or not
			// better safe then sorry 🤷‍♂️
			bool isLive = false;
			{
				std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
				isLive = (AppState::state.currentView == AppState::CurrentView::LIVE);
			}

			if (isLive)
			{
				int todayScore = 0;
				std::vector<int> hourlyScores;
				{
					std::lock_guard<std::mutex> pLock(AppState::patternAnalyzerMutex);
					std::vector<int> scores = AppState::patternAnalyzer.calculateProductivityScore(true, true);
					if (!scores.empty())
						todayScore = scores[0];
					hourlyScores = AppState::patternAnalyzer.calculateProductivityScore_Hourly(true, true);
				}

				*db << "delete from productivity_scores_daily;";
				*db << "insert or replace into productivity_scores_daily (score) values (?);" << todayScore;

				for (int hour = 0; hour < 24; hour++)
					*db << "insert or replace into productivity_scores_hourly (hour, score) values (?, ?);"
						<< hour << hourlyScores[hour];
			}

			std::vector<PendingQuery> queriesToRun;
			{
				std::lock_guard<std::mutex> lock(pendingQueriesMutex);
				queriesToRun = std::move(pendingQueries);
				pendingQueries.clear();
			}
			for (const auto &q : queriesToRun)
			{
				try
				{
					auto binder = *db << q.sql;
					for (const auto &param : q.params)
						binder << param;
				}
				catch (const std::exception &e)
				{
					std::cerr << "[DB QUEUED EXECUTE ERROR] " << e.what() << " | SQL: " << q.sql << std::endl;
					Logger::log("[DB QUEUED EXECUTE ERROR] " + std::string(e.what()) + " | SQL: " + q.sql);
				}
			}

			*db << "COMMIT;";
			*db << "PRAGMA wal_checkpoint(PASSIVE);";

			auto nowSystem = std::chrono::system_clock::now();
			uint64_t t = std::chrono::duration_cast<std::chrono::milliseconds>(nowSystem.time_since_epoch()).count();
			newName = convertToDate_DDMMYY(t) + ".db";
			needsRollover = (newName != fileName);
		}

		if (needsRollover)
		{
			{
				std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
				AppState::state.timeLog_PerApp.clear();
				AppState::state.timeLog_PerTab.clear();
				AppState::state.timeLog_PerProject.clear();
				AppState::state.switchHistory.clear();
				AppState::state.limitTimeBase.clear();
				AppState::state.goalTimeBase.clear();
			}
			EventHub::emit(Event::MIDNIGHT_ROLLOVER);
			std::lock_guard<std::mutex> dbLock(dbQueryMutex);
			initDatabase(false);
		}

		int flushInterval = AppState::configManager.getConfig("db-flush-interval", 10000);
		int iterations = flushInterval / 100;
		if (iterations <= 0)
			iterations = 1;
		for (int i = 0; i < iterations && running; i++)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	// Final flush on shutdown
	std::vector<PendingQuery> queriesToRun;
	{
		std::lock_guard<std::mutex> lock(pendingQueriesMutex);
		queriesToRun = std::move(pendingQueries);
		pendingQueries.clear();
	}
	if (!queriesToRun.empty())
	{
		std::lock_guard<std::mutex> dbLock(dbQueryMutex);
		try
		{
			*db << "BEGIN;";
			for (const auto &q : queriesToRun)
			{
				auto binder = *db << q.sql;
				for (const auto &param : q.params)
				{
					binder << param;
				}
			}
			*db << "COMMIT;";
			*db << "PRAGMA wal_checkpoint(PASSIVE);";
		}
		catch (const std::exception &e)
		{
			std::cerr << "[DB SHUTDOWN EXECUTE ERROR] " << e.what() << std::endl;
			Logger::log("[DB SHUTDOWN EXECUTE ERROR] " + std::string(e.what()));
		}
	}
}
