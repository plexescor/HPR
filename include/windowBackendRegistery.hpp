#pragma once

#include <functional>
#include <string>
#include <vector>

struct WindowBackend
{
	std::string name;
	std::string ownerAuthor;
	std::string ownerExtension;

	std::function<bool(const std::string &desktopEnvironment)> matchesEnvironment;
	std::function<void()> initialize;

	std::function<std::string()> getCurrentWindow;
	std::function<std::string()> getCurrentTitle;
	std::function<std::string()> getCurrentPid;

	bool nativeBackend = true;
};

extern std::vector<WindowBackend> registeredBackends;

void registerBackend(const WindowBackend &backend);

WindowBackend *getBackendByName(const std::string &name);

void unregisterNonNativeBackends();

void unregisterBackendByOwner(const std::string &authorName, const std::string &extensionName);