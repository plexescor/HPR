#pragma once
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>

class LimitsManager
{
  public:
	LimitsManager();
	~LimitsManager();

	void run();
	void limitReached(const std::string &appName);

	void setLimit(const std::string &appName, int minutes);
	void setGoal(const std::string &appName, int minutes);

	std::string getLimitRemaining(const std::string &appName, uint64_t currentDurationMs, int limitMins);
	std::string getGoalRemaining(const std::string &appName, uint64_t currentDurationMs, int goalMins);

  private:
	void checkLoop();

  private:
	std::atomic<bool> running{false};
	std::thread checkerThread;

	// Track which notifications have already been sent today
	// Maps raw app names to notification flags
	std::map<std::string, bool> limitWarningSent;
	std::map<std::string, bool> limitReachedSent;
	std::map<std::string, bool> goalWarningSent;
	std::map<std::string, bool> goalReachedSent;

	std::map<std::string, uint64_t> limitTimeBase;
	std::map<std::string, uint64_t> goalTimeBase;
	std::map<std::string, bool> killSent;

	std::chrono::steady_clock::time_point lastGlobalKillTime;

	std::mutex limitsMutex;
};
