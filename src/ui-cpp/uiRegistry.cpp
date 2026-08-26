#include "uiRegistry.hpp"
#include <slint-interpreter.h>

/*
	Registers the handle of slint's window which is supplied as a param by the caller
*/
void UiRegistry::registerInstance(slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> handle)
{
	std::lock_guard<std::mutex> lock(mutex);
	activeInstance = handle;
	active = true;
}

/*
	Returns the handle of the active instance of slint Ui
*/
slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> UiRegistry::getInstance()
{
	std::lock_guard<std::mutex> lock(mutex);
	return activeInstance;
}

/*
	Returns if the Ui is active or not
*/
bool UiRegistry::isActive() { return active; }