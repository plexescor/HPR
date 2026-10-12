#include "databaseManager.hpp"
#include "appState.hpp"
#include "logger.hpp"

bool DatabaseManager::loadStateFromDB()
{
	try
	{

		// No mutex needed because it runs before everything else

		// Load app_usage into AppState
		*db << "select name, duration from app_usage;" >> [](std::string name, uint64_t duration)
		{ AppState::state.timeLog_PerApp[name] += duration; };

		*db << "select name, duration from tab_usage;" >> [](std::string name, uint64_t duration)
		{ AppState::state.timeLog_PerTab[name] += duration; };

		*db << "select name, duration from project_usage;" >> [](std::string name, uint64_t duration)
		{ AppState::state.timeLog_PerProject[name] += duration; };

		// Load switch_history into AppState
		*db << "select fromWindow, toWindow, timeStamp from switch_history;" >>
			[](std::string from, std::string to, long long ts)
		{ AppState::state.switchHistory[{from, to}].push_back((uint64_t)ts); };

		// Load app_limits & app_goals into AppState
		*db << "select name, minutes from app_limits;" >> [](std::string name, int minutes)
		{ AppState::state.appLimits[name] = minutes; };

		*db << "select name, minutes from app_goals;" >> [](std::string name, int minutes)
		{ AppState::state.appGoals[name] = minutes; };

		*db << "select name, base_ms from limit_bases;" >> [](std::string name, uint64_t base)
		{ AppState::state.limitTimeBase[name] = base; };

		*db << "select name, base_ms from goal_bases;" >> [](std::string name, uint64_t base)
		{ AppState::state.goalTimeBase[name] = base; };
	}
	catch (const std::exception &e)
	{
		std::cerr << "[ERROR IN DB LOAD FROM DISK] " << e.what() << std::endl;
		Logger::log("[ERROR IN DB LOAD FROM DISK] " + std::string(e.what()));
		return false;
	}

	return true;
}