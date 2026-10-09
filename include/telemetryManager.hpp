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
};
