#include "windowBackendRegistery.hpp"
#include <functional>
#include <string>
#include <vector>

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