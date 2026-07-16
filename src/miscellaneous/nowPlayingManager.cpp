#include "nowPlayingManager.hpp"
#include "appState.hpp"
#include "logger.hpp"
#include "netUtilities.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>

namespace
{
const std::string FIREBASE_HOST = "humanpatternrecorder-default-rtdb.firebaseio.com";
std::string lastUploadedTitle = "";
} // namespace

void NowPlayingManager::init()
{
	// Start background thread running every 15s
	std::thread(
		[]()
		{
			// Wait 5 seconds after startup for database initialization to
			// settle
			std::this_thread::sleep_for(std::chrono::seconds(5));
			while (true)
			{
				runCycle();
				std::this_thread::sleep_for(std::chrono::seconds(15));
			}
		})
		.detach();
}

std::string NowPlayingManager::toLower(const std::string &s)
{
	std::string result = s;
	std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });
	return result;
}

void NowPlayingManager::runCycle()
{
	try
	{
		// Password from config (default "69" if not set)
		std::string password = AppState::configManager.getConfig<std::string>("firebase-password", "69");

		std::string currentTitle = "";
		{
			std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
			currentTitle = AppState::state.currentTitle;
		}

		std::string lowerTitle = toLower(currentTitle);
		bool isWatchingYouTube = (lowerTitle.find("youtube") != std::string::npos);

		std::string cleanTitle = "";
		std::string videoUrl = "";

		if (isWatchingYouTube)
		{
			// Clean title: strip "- YouTube" and subsequent browser suffixes
			size_t ytPos = lowerTitle.find(" - youtube");
			if (ytPos != std::string::npos)
			{
				cleanTitle = currentTitle.substr(0, ytPos);
			}
			else
			{
				cleanTitle = currentTitle;
			}

			// URL Encode the clean title to build a search URL
			videoUrl = "https://www.youtube.com/results?search_query=" + urlEncode(cleanTitle);
		}

		// Only upload to database if the title has changed to minimize DB
		// writes
		if (cleanTitle != lastUploadedTitle)
		{
			std::map<std::string, std::string> headers = {{"Content-Type", "application/json"}};

			// Step 1: write the secret node (rules allow this when value ===
			// password)
			std::string secretBody = "\"" + escapeJsonString(password) + "\"";
			auto secretResp = NativeNet::httpPut(FIREBASE_HOST, "/now_playing/secret.json", secretBody, true, headers);

			if (secretResp.second >= 200 && secretResp.second < 300)
			{
				// Step 2: write data (rules check sibling secret node ===
				// password)
				std::string dataBody = "{\"title\":\"" + escapeJsonString(cleanTitle) + "\",\"url\":\"" +
									   escapeJsonString(videoUrl) + "\"}";
				auto dataResp = NativeNet::httpPut(FIREBASE_HOST, "/now_playing/data.json", dataBody, true, headers);
				if (dataResp.second >= 200 && dataResp.second < 300)
				{
					lastUploadedTitle = cleanTitle;
					Logger::log("[NowPlaying] Uploaded successfully: " + cleanTitle);
				}
				else
				{
					Logger::log("[NowPlaying] Data write failed with status: " + std::to_string(dataResp.second));
				}
			}
			else
			{
				Logger::log("[NowPlaying] Secret write failed with status: " + std::to_string(secretResp.second));
			}
		}

		// 2. Perform GET request to fetch the current YouTube status
		// All clients (including the dev client) perform this so the About view
		// is updated
		auto getResponse = NativeNet::httpGet(FIREBASE_HOST, "/now_playing/data.json", true);
		if (getResponse.second >= 200 && getResponse.second < 300)
		{
			std::string responseBody = getResponse.first;
			std::string fetchedTitle = "";
			std::string fetchedUrl = "";

			if (responseBody != "null" && !responseBody.empty())
			{
				fetchedTitle = extractJsonValue(responseBody, "title");
				fetchedUrl = extractJsonValue(responseBody, "url");
			}

			{
				std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
				AppState::state.nowPlayingTitle = fetchedTitle;
				AppState::state.nowPlayingUrl = fetchedUrl;
			}
		}
		else
		{
			Logger::log("[NowPlaying] Failed to fetch data: status " + std::to_string(getResponse.second));
		}
	}
	catch (const std::exception &e)
	{
		Logger::log("[NowPlaying] Exception in runCycle: " + std::string(e.what()));
	}
	catch (...)
	{
		Logger::log("[NowPlaying] Unknown exception in runCycle");
	}
}

std::string NowPlayingManager::extractJsonValue(const std::string &json, const std::string &key)
{
	size_t keyPos = json.find("\"" + key + "\"");
	if (keyPos == std::string::npos)
		return "";

	size_t colonPos = json.find(":", keyPos);
	if (colonPos == std::string::npos)
		return "";

	size_t startQuote = json.find("\"", colonPos);
	if (startQuote == std::string::npos)
		return "";

	size_t endQuote = json.find("\"", startQuote + 1);
	if (endQuote == std::string::npos)
		return "";

	// Parse escape sequences in the JSON string
	std::string value = json.substr(startQuote + 1, endQuote - startQuote - 1);
	std::string cleanValue = "";
	for (size_t i = 0; i < value.length(); ++i)
	{
		if (value[i] == '\\' && i + 1 < value.length())
		{
			char next = value[i + 1];
			if (next == '"')
				cleanValue += '"';
			else if (next == '\\')
				cleanValue += '\\';
			else if (next == 'n')
				cleanValue += '\n';
			else if (next == 'r')
				cleanValue += '\r';
			else if (next == 't')
				cleanValue += '\t';
			else
				cleanValue += next;
			i++;
		}
		else
		{
			cleanValue += value[i];
		}
	}
	return cleanValue;
}

std::string NowPlayingManager::urlEncode(const std::string &value)
{
	std::ostringstream escaped;
	escaped << std::hex;
	for (char c : value)
	{
		if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~')
		{
			escaped << c;
		}
		else if (c == ' ')
		{
			escaped << '+';
		}
		else
		{
			escaped << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c));
		}
	}
	return escaped.str();
}

std::string NowPlayingManager::escapeJsonString(const std::string &s)
{
	std::string escaped;
	for (char c : s)
	{
		if (c == '"')
			escaped += "\\\"";
		else if (c == '\\')
			escaped += "\\\\";
		else if (c == '\n')
			escaped += "\\n";
		else if (c == '\r')
			escaped += "\\r";
		else if (c == '\t')
			escaped += "\\t";
		else
			escaped += c;
	}
	return escaped;
}
