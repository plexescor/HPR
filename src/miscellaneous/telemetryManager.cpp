#include "telemetryManager.hpp"
#include "appEvents.hpp"
#include "appState.hpp"
#include "logger.hpp"
#include "netUtilities.hpp"
#include "timeUtils.hpp"

#include <chrono>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <print>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

namespace
{
const std::string FIREBASE_HOST = "humanpatternrecorder-default-rtdb.firebaseio.com";
}

std::string TelemetryManager::generateUUID()
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 15);
	std::uniform_int_distribution<> dis2(8, 11);

	std::stringstream ss;
	ss << std::hex;
	for (int i = 0; i < 8; i++)
		ss << dis(gen);
	ss << "-";
	for (int i = 0; i < 4; i++)
		ss << dis(gen);
	ss << "-4"; // UUID version 4
	for (int i = 0; i < 3; i++)
		ss << dis(gen);
	ss << "-";
	ss << dis2(gen);
	for (int i = 0; i < 3; i++)
		ss << dis(gen);
	ss << "-";
	for (int i = 0; i < 12; i++)
		ss << dis(gen);
	return ss.str();
}

void TelemetryManager::init()
{
	
}

void TelemetryManager::sendDemographicContinent(const std::string &continent)
{
	try
	{
		// Get or generate the persistent UUID — same one used by telemetry
		std::string userId = AppState::configManager.getConfig<std::string>("user-id", "");
		if (userId.empty())
		{
			userId = generateUUID();
			AppState::configManager.setConfig("user-id", userId);
		}

		auto now        = std::chrono::system_clock::now();
		uint64_t ts     = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

		std::string body = "{\"userId\":\"" + userId +
		                   "\",\"continent\":\"" + continent +
		                   "\",\"timestamp\":" + std::to_string(ts) + "}";

		std::map<std::string, std::string> headers = {{"Content-Type", "application/json"}};
		std::string path = "/demographics/" + userId + ".json";

		auto response = NativeNet::httpPut(FIREBASE_HOST, path, body, true, headers);
		if (response.second >= 200 && response.second < 300)
		{
			Logger::log("[Demographics] Continent submitted successfully: " + continent);
		}
		else
		{
			Logger::log("[Demographics] Submission failed with code: " + std::to_string(response.second));
		}
	}
	catch (const std::exception &e)
	{
		Logger::log("[Demographics] Error in sendDemographicContinent: " + std::string(e.what()));
	}
	catch (...)
	{
		Logger::log("[Demographics] Unknown error in sendDemographicContinent");
	}
}