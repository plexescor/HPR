#pragma once

#include <slint-interpreter.h>

#include <atomic>
#include <mutex>
#include <optional>

class UiRegistry
{
  public:
	static void registerInstance(slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> handle);
	static slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> getInstance();
	static bool isActive();

  private:
	static inline std::mutex mutex;
	static inline slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> activeInstance;
	static inline std::atomic<bool> active{false};
};