#pragma once

#include <string>
#include <vector>

class TelemetryManager
{
  public:
	static void init();
	static void checkAndSend();
	static void sendDemographicContinent(const std::string &continent);

  private:
	static std::string generateUUID();
	static void privilegedAggregationCycle();
	static int countJsonTopLevelKeys(const std::string &json);
	static bool jsonHasTopLevelKey(const std::string &json, const std::string &key);
	static int parseCountFromSummary(const std::string &json, const std::string &prefix);
	static std::vector<std::string> extractTopLevelKeys(const std::string &json);
};
