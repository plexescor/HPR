// HPR.hpp
#pragma once

#include "appState.hpp"
#include "uiModelManager.hpp"

// Slint stuff
#include "app-window.h"
#include "extensionManager.hpp"
#include <slint.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

class HPR
{
  public:
	HPR(ExtensionManager *extMgr = nullptr);
	~HPR();
	void show();
	void hide();
	void run(); // blocking call i believe
	void quit();
	void setUiImage(const std::string &propertyName, const slint::SharedPixelBuffer<slint::Rgba8Pixel> &pixelBuffer);

  private:
	void trackingLoop(); // runs on separate thread so that it polls shit
						 // continously (correct spelling?)
	void saveWindowGeometry();

  private:
	ExtensionManager *extManager;

	slint::ComponentHandle<MainWindow> ui;
	std::atomic<bool> running{true};
	std::atomic<bool> paused{false};
	std::mutex pauseMutex;
	std::condition_variable pauseCv;

	std::thread tracker;

	UiModelManager modelManager;

	size_t errorId;
	std::string activeGuiError = "";
	std::chrono::steady_clock::time_point errorTimestamp;
};