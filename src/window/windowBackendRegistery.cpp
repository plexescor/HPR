#include "windowBackendRegistery.hpp"
#include <functional>
#include <string>
#include <vector>
#include <algorithm>

std::vector<WindowBackend> registeredBackends;

// WindowBackend is a struct
void registerBackend(const WindowBackend &backend) { registeredBackends.push_back(backend); }

WindowBackend *getBackendByName(const std::string &name)
{
	for (auto &backend : registeredBackends)
	{
		if (name.contains(backend.name))
			return &backend;
	}

	return nullptr;
}

void unregisterNonNativeBackends()
{
	registeredBackends.erase(
		std::remove_if(registeredBackends.begin(), registeredBackends.end(),
					   [](const WindowBackend &b) { return !b.nativeBackend; }),
		registeredBackends.end());
}

void unregisterBackendByOwner(const std::string &authorName, const std::string &extensionName)
{
	registeredBackends.erase(
		std::remove_if(registeredBackends.begin(), registeredBackends.end(),
					   [&](const WindowBackend &b)
					   {
						   return !b.nativeBackend && b.ownerAuthor == authorName &&
								  b.ownerExtension == extensionName;
					   }),
		registeredBackends.end());
}