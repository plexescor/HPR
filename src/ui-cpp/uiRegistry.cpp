#include "uiRegistry.hpp"
#include <slint-interpreter.h>

void UiRegistry::registerInstance(slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> handle) 
{
    std::lock_guard<std::mutex> lock(mutex);
    activeInstance = handle;
    active = true;
}

slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> UiRegistry::getInstance() 
{
    std::lock_guard<std::mutex> lock(mutex);
    return activeInstance;
}

bool UiRegistry::isActive() 
{
    return active;
}