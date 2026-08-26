#include "windowBackendRegistery.hpp"
#include <functional>
#include <string>
#include <vector>
#include <algorithm>

std::vector<WindowBackend> registeredBackends;

/*
	Registers a new backend for core window detection done by HPR
*/
void registerBackend(const WindowBackend &backend) { registeredBackends.push_back(backend); }

/*
	Returns a backend by referencing its name via a string
*/
WindowBackend *getBackendByName(const std::string &name)
{
	for (auto &backend : registeredBackends)
	{
		if (name.contains(backend.name))
			return &backend;
	}

	return nullptr;
}

/*
	Unregisters all the backends registered by lua extensions
*/
void unregisterNonNativeBackends()
{
	registeredBackends.erase(
		std::remove_if(registeredBackends.begin(), registeredBackends.end(),
					   [](const WindowBackend &b) { return !b.nativeBackend; }),
		registeredBackends.end());
}

/*
	Unregisters only the backend registered by a specific lua extension
*/
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