#include "uiModelManager.hpp"
#include "aliasManager.hpp"
#include "appState.hpp"
#include "limitsManager.hpp"
#include "logger.hpp"
#include "timeUtils.hpp"
#include "timelineManager.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <mutex>

// Slint stuff
// #include "app-window.h"
#include <slint.h>

// second constructor for interpreter mode
UiModelManager::UiModelManager(slint::ComponentHandle<slint::interpreter::ComponentInstance> &ui_handle)
	: ui_interp(ui_handle)
{
	timeLogModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
	timeLogModelTab_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
	timeLogModelProject_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
	switchHistoryModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
	extensionsModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
	rawAppsModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
	timelineBlocksModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
	timelineMarkersModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
	themesListModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
	themePreviewsModel_interp = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();

	currentSelectedTheme = AppState::configManager.getConfig("custom-theme", std::string("Default"));
	lastLoadedPreviewThemePtr = std::make_shared<std::string>("");

	slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> weak(ui_interp.value());
	slint::invoke_from_event_loop(
		[weak, this]()
		{
			if (auto handle = weak.lock())
			{
				auto setPropSafe = [&](const char *name, const slint::interpreter::Value &val)
				{
					if ((*handle)->get_property(name).has_value())
					{
						(*handle)->set_property(name, val);
					}
				};

				setPropSafe("timePerApp_S", slint::interpreter::Value(timeLogModel_interp));
				setPropSafe("timePerTab_S", slint::interpreter::Value(timeLogModelTab_interp));
				setPropSafe("timePerProject_S", slint::interpreter::Value(timeLogModelProject_interp));
				setPropSafe("switchHistory_S", slint::interpreter::Value(switchHistoryModel_interp));
				setPropSafe("loadedExtensions_S", slint::interpreter::Value(extensionsModel_interp));
				setPropSafe("rawApps_S", slint::interpreter::Value(rawAppsModel_interp));
				setPropSafe("timelineBlocks_S", slint::interpreter::Value(timelineBlocksModel_interp));
				setPropSafe("timelineMarkers_S", slint::interpreter::Value(timelineMarkersModel_interp));
				setPropSafe("themesList_S", slint::interpreter::Value(themesListModel_interp));
				setPropSafe("themePreviewImages_S", slint::interpreter::Value(themePreviewsModel_interp));

				// Set initial theme values immediately!
				setPropSafe("currentTheme_S", slint::interpreter::Value(slint::SharedString(currentSelectedTheme)));
				std::string activeTheme = AppState::configManager.getConfig("custom-theme", std::string("Default"));
				setPropSafe("activeTheme_S", slint::interpreter::Value(slint::SharedString(activeTheme)));

				std::string currentAuthor = AppState::themeManager.availableThemes_Author.contains(currentSelectedTheme)
												? AppState::themeManager.availableThemes_Author[currentSelectedTheme]
												: "Unknown";
				std::string activeAuthor = AppState::themeManager.availableThemes_Author.contains(activeTheme)
											   ? AppState::themeManager.availableThemes_Author[activeTheme]
											   : "Unknown";
				setPropSafe("currentThemeAuthor_S", slint::interpreter::Value(slint::SharedString(currentAuthor)));
				setPropSafe("activeThemeAuthor_S", slint::interpreter::Value(slint::SharedString(activeAuthor)));
				setPropSafe("themeAuthor_S", slint::interpreter::Value(slint::SharedString(currentAuthor)));
			}
		});
}

/*
	Updates the interpreted Ui's core data
*/
void UiModelManager::update_Interpreted(
	const std::map<std::string, uint64_t> &rawTimeLog, const std::map<std::string, uint64_t> &rawTimeLog_Tab,
	const std::map<std::string, uint64_t> &rawTimeLog_Project,
	const std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> &rawHistory,
	std::string &rawCurrentWindowName, uint64_t &totalTrackedTime, uint64_t &totalTrackedTime_Tab,
	uint64_t &totalTrackedTime_Project, AliasManager &aliasManager)
{
	std::string currentWindowName = aliasManager.getAlias(rawCurrentWindowName);

	//----------------------TIME
	// LOG-----------------------------------------------

	std::map<std::string, uint64_t> translatedTimeLog;
	std::map<std::string, uint64_t> translatedTimeLog_Tab;
	std::map<std::string, uint64_t> translatedTimeLog_Project;

	for (const auto &[raw, duration] : rawTimeLog)
	{
		translatedTimeLog[aliasManager.getAlias(raw)] += duration;
		totalTrackedTime += duration;
	}

	bool isTabView;
	{
		std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
		isTabView = AppState::state.useTabView;
	}

	// if tab view is on, we show raw data without aliasing, because it is per
	// *tab* and not per site, so aliasing would just mess things up. If tab
	// view is off, we alias as normal
	if (isTabView)
	{
		for (const auto &[raw, duration] : rawTimeLog_Tab)
		{
			translatedTimeLog_Tab[raw] += duration;
			totalTrackedTime_Tab += duration;
		}
	}
	else
	{
		for (const auto &[raw, duration] : rawTimeLog_Tab)
		{
			translatedTimeLog_Tab[aliasManager.getAlias_Tab(raw)] += duration;
			totalTrackedTime_Tab += duration;
		}
	}

	// Projects
	bool isRawView;
	{
		std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
		isRawView = AppState::state.isRawProjectView;
	}

	// if its raw view, just raw dog it
	if (isRawView)
	{
		for (const auto &[raw, duration] : rawTimeLog_Project)
		{
			std::string cleaned = raw;
			if (auto pos = cleaned.find("jetbrains: "); pos != std::string::npos)
			{
				cleaned.erase(pos, std::string("jetbrains: ").length());
			}
			translatedTimeLog_Project[cleaned] += duration;
			totalTrackedTime_Project += duration;
		}
	}
	// Parse it first and then find an aliases if present
	else
	{
		// How does this parse? interesting question
		// Lets take an example: test.cpp - HPR - Visual Studio Code is the
		// window title what it will do is first check if it ends with " -
		// Visual Studio Code", if it does, it will remove that part, so now we
		// have "test.cpp - HPR" Then it will find the last occurrence of " - ",
		// which is the separator between project keep in mind it finds " - "
		// and not just "-", because project names can have dashes in them, but
		// the separator is always " - " no person uses " - " in their project
		// names, that would be shit, so this is a safe separator to use"
		for (const auto &[raw, duration] : rawTimeLog_Project)
		{
			if (!raw.empty() && (raw.contains("Visual Studio Code") || raw.contains("vscode") || raw.contains("Code")))
			{
				std::string middleMan;

				const std::string suffix = " - Visual Studio Code";
				std::string stripped = raw;

				if (stripped.size() >= suffix.size() && stripped.substr(stripped.size() - suffix.size()) == suffix)
					stripped = stripped.substr(0, stripped.size() - suffix.size());

				// rfind last " - "
				size_t pos = stripped.rfind(" - ");
				if (pos == std::string::npos)
					middleMan = stripped; // no separator, return raw
				else
					middleMan = stripped.substr(pos + 3);

				translatedTimeLog_Project[aliasManager.getAlias_Project(middleMan)] += duration;
				totalTrackedTime_Project += duration;
			}

			// JetBrains: class starts with "jetbrains-", title is "ProjectName
			// – file [module]" or just "ProjectName" JetBrains: class starts
			// with "jetbrains-", title is "ProjectName – file [module]" or just
			// "ProjectName" fragile as fuck
			else if (raw.contains("jetbrains: "))
			{
				std::string cleaned = raw;

				// Find the position of the substring
				if (auto pos = cleaned.find("jetbrains: "); pos != std::string::npos)
				{
					// Erase using starting position and length
					cleaned.erase(pos,
								  std::string("jetbrains: ").length()); // fuck hardoced
				}

				const std::string emDash = " \xe2\x80\x93 "; // em dash
				size_t pos = cleaned.find(emDash);
				std::string middleMan = (pos == std::string::npos) ? cleaned : cleaned.substr(0, pos);

				translatedTimeLog_Project[aliasManager.getAlias_Project(middleMan)] += duration;
				totalTrackedTime_Project += duration;
			}
		}
	}

	// interpreter uses Value, not typed structs
	std::vector<slint::interpreter::Value> slintVec_TimeLog;
	std::vector<slint::interpreter::Value> slintVec_TimeLog_Tab;
	std::vector<slint::interpreter::Value> slintVec_TimeLog_Project;

	for (const auto &[name, duration] : translatedTimeLog)
	{
		slint::interpreter::Struct entry;
		entry.set_field("name", slint::interpreter::Value(slint::SharedString(name)));
		entry.set_field("duration", slint::interpreter::Value(slint::SharedString(formatTime_HHMMSS(duration))));
		entry.set_field("duration_i", slint::interpreter::Value((double)duration));
		slintVec_TimeLog.push_back(slint::interpreter::Value(entry));
	}

	for (const auto &[name, duration] : translatedTimeLog_Tab)
	{
		slint::interpreter::Struct entry;
		entry.set_field("name", slint::interpreter::Value(slint::SharedString(name)));
		entry.set_field("duration", slint::interpreter::Value(slint::SharedString(formatTime_HHMMSS(duration))));
		entry.set_field("duration_i", slint::interpreter::Value((double)duration));
		slintVec_TimeLog_Tab.push_back(slint::interpreter::Value(entry));
	}

	for (const auto &[name, duration] : translatedTimeLog_Project)
	{
		slint::interpreter::Struct entry;
		entry.set_field("name", slint::interpreter::Value(slint::SharedString(name)));
		entry.set_field("duration", slint::interpreter::Value(slint::SharedString(formatTime_HHMMSS(duration))));
		entry.set_field("duration_i", slint::interpreter::Value((double)duration));
		slintVec_TimeLog_Project.push_back(slint::interpreter::Value(entry));
	}

	// Sort by duration_i descending
	std::sort(
		slintVec_TimeLog.begin(), slintVec_TimeLog.end(),
		[](const slint::interpreter::Value &a, const slint::interpreter::Value &b)
		{
			auto sa = a.to_struct();
			auto sb = b.to_struct();

			if (sa && sb)
			{
				// get_field returns an optional Value, then we convert to
				// number, then dereference
				double da =
					sa->get_field("duration_i").value_or(slint::interpreter::Value(0.0)).to_number().value_or(0.0);
				double db =
					sb->get_field("duration_i").value_or(slint::interpreter::Value(0.0)).to_number().value_or(0.0);
				return da > db;
			}
			return false;
		});

	std::sort(
		slintVec_TimeLog_Tab.begin(), slintVec_TimeLog_Tab.end(),
		[](const slint::interpreter::Value &a, const slint::interpreter::Value &b)
		{
			auto sa = a.to_struct();
			auto sb = b.to_struct();

			if (sa && sb)
			{
				// get_field returns an optional Value, then we convert to
				// number, then dereference
				double da =
					sa->get_field("duration_i").value_or(slint::interpreter::Value(0.0)).to_number().value_or(0.0);
				double db =
					sb->get_field("duration_i").value_or(slint::interpreter::Value(0.0)).to_number().value_or(0.0);
				return da > db;
			}
			return false;
		});

	std::sort(
		slintVec_TimeLog_Project.begin(), slintVec_TimeLog_Project.end(),
		[](const slint::interpreter::Value &a, const slint::interpreter::Value &b)
		{
			auto sa = a.to_struct();
			auto sb = b.to_struct();

			if (sa && sb)
			{
				// get_field returns an optional Value, then we convert to
				// number, then dereference
				double da =
					sa->get_field("duration_i").value_or(slint::interpreter::Value(0.0)).to_number().value_or(0.0);
				double db =
					sb->get_field("duration_i").value_or(slint::interpreter::Value(0.0)).to_number().value_or(0.0);
				return da > db;
			}
			return false;
		});

	// Raw Apps extraction for interpreted mode with Limits & Goals from
	// AppState
	std::vector<slint::interpreter::Value> slintVec_RawApps;
	{
		std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
		for (const auto &[raw, duration] : rawTimeLog)
		{
			int limit = 0;
			int goal = 0;
			if (AppState::state.appLimits.count(raw))
				limit = AppState::state.appLimits.at(raw);
			if (AppState::state.appGoals.count(raw))
				goal = AppState::state.appGoals.at(raw);

			std::string limit_rem;
			std::string goal_rem;
			if (AppState::limitsManager)
			{
				limit_rem = AppState::limitsManager->getLimitRemaining(raw, duration, limit);
				goal_rem = AppState::limitsManager->getGoalRemaining(raw, duration, goal);
			}

			// get display name using aliasManager under lua/state mutex lock
			// (already locked via stateMutex)
			std::string displayName = aliasManager.getAlias(raw);

			slint::interpreter::Struct entry;
			entry.set_field("name", slint::interpreter::Value(slint::SharedString(displayName)));
			entry.set_field("raw_name", slint::interpreter::Value(slint::SharedString(raw)));
			entry.set_field("limit_mins", slint::interpreter::Value((double)limit));
			entry.set_field("goal_mins", slint::interpreter::Value((double)goal));
			entry.set_field("limit_remaining", slint::interpreter::Value(slint::SharedString(limit_rem)));
			entry.set_field("goal_remaining", slint::interpreter::Value(slint::SharedString(goal_rem)));
			slintVec_RawApps.push_back(slint::interpreter::Value(entry));
		}
	}

	//----------------------------------SWITCH
	// HISTORY---------------------------------------

	struct TempSwitchHistory
	{
		std::string from;
		std::string to;
		uint64_t maxVal;
	};

	std::vector<TempSwitchHistory> tempSwitchVec;

	for (const auto &[route, timeStampS] : rawHistory)
	{
		if (timeStampS.empty())
			continue;
		const auto &[from, to] = route;
		const auto maxVal = *std::max_element(timeStampS.begin(), timeStampS.end());
		tempSwitchVec.push_back({from, to, maxVal});
	}

	std::sort(tempSwitchVec.begin(), tempSwitchVec.end(),
			  [](const TempSwitchHistory &a, const TempSwitchHistory &b) { return a.maxVal > b.maxVal; });

	std::vector<slint::interpreter::Value> slintVec_SwitchHistory;
	size_t switchHistoryLimit = std::min(tempSwitchVec.size(), size_t(100));

	for (size_t i = 0; i < switchHistoryLimit; ++i)
	{
		const auto &item = tempSwitchVec[i];
		slint::interpreter::Struct entry;
		entry.set_field("fromWindow", slint::interpreter::Value(slint::SharedString(aliasManager.getAlias(item.from))));
		entry.set_field("toWindow", slint::interpreter::Value(slint::SharedString(aliasManager.getAlias(item.to))));
		entry.set_field("maxTimeStamp",
						slint::interpreter::Value(slint::SharedString(convertToTime_HHMMSS_12(item.maxVal))));
		slintVec_SwitchHistory.push_back(slint::interpreter::Value(entry));
	}

	//-----------------Actually pushing changes to slint for
	// rendering-----------------------

	if (!ui_interp.has_value())
	{
		return;
	}

	std::string theme = AppState::configManager.getConfig<std::string>("theme", "dark");
	bool isDark = (theme != "light");

	slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> weak(ui_interp.value());
	slint::invoke_from_event_loop(
		[weak, slintVec_TimeLog, slintVec_TimeLog_Tab, slintVec_TimeLog_Project, slintVec_SwitchHistory,
		 slintVec_RawApps, totalTrackedTime, totalTrackedTime_Tab, totalTrackedTime_Project, currentWindowName, theme,
		 isDark, this]()
		{
			if (auto handle = weak.lock())
			{
				bool autostartVal = AppState::configManager.getConfig<bool>("autostart", false);
				bool headlessVal = AppState::configManager.getConfig<bool>("headless-mode", false);
				bool trueHeadlessVal = AppState::configManager.getConfig<bool>("true-headless-mode", false);
				bool telemetryVal = AppState::configManager.getConfig<bool>("anonymous-telemetry", false);
				bool noTitleBarVal = AppState::configManager.getConfig<bool>("no-title-bar", false);
				bool showMiscImageVal = AppState::configManager.getConfig<bool>("show-misc-image-panel", false);
				float cornerRoundnessVal = AppState::configManager.getConfig<float>("corner-roundness", 0.5f);
				float uiScaleVal = AppState::configManager.getConfig<float>("ui-scale", 0.5f);
				bool showPromptVal =
					AppState::configManager.isFirstLaunch() && !AppState::configManager.isTelemetryPromptAnswered();

				bool killAppsVal = AppState::configManager.getConfig<bool>("kill-apps", true);
				bool hwAccelVal = AppState::configManager.getConfig<bool>("hardware-acceleration", true);
				bool allowCustomVal = AppState::configManager.getConfig<bool>("allow-custom-backends", false);
				bool allowNetworkVal = AppState::configManager.getConfig<bool>("allow-network-activity", true);
				bool allowUpdateCheckingVal = AppState::configManager.getConfig<bool>("allow-update-checking", true);
				bool updateAvailableVal = false;
				bool isUpdatingVal = false;
				float updateProgressVal = 0.0f;
				std::string updateStatusTextVal = "Update HPR";
				{
					std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
					updateAvailableVal = AppState::state.updateAvailable;
					isUpdatingVal = AppState::state.isUpdating;
					updateProgressVal = AppState::state.updateProgress;
					updateStatusTextVal = AppState::state.updateStatusText;
				}
				bool allowNativeLibrariesVal = AppState::configManager.getConfig<bool>("allow-native-libraries", false);
				bool fullBrightnessBarsVal = AppState::configManager.getConfig<bool>("full-brightness-bars", true);
				bool showContinentPromptVal = allowNetworkVal && !AppState::configManager.isContinentPromptAnswered();

				bool allowSidebarCustomizationVal =
					AppState::configManager.getConfig<bool>("allow-sidebar-customization", false);
				bool hideSidebarReloadVal = AppState::configManager.getConfig<bool>("hide-sidebar-reload", false);
				bool hideSidebarLimitsVal = AppState::configManager.getConfig<bool>("hide-sidebar-limits", false);
				bool hideSidebarHistoryVal = AppState::configManager.getConfig<bool>("hide-sidebar-history", false);
				bool hideSidebarTimelineVal = AppState::configManager.getConfig<bool>("hide-sidebar-timeline", false);
				bool hideSidebarTabVal = AppState::configManager.getConfig<bool>("hide-sidebar-tab", false);
				bool hideSidebarProjectVal = AppState::configManager.getConfig<bool>("hide-sidebar-project", false);
				bool hideSidebarInsightsVal = AppState::configManager.getConfig<bool>("hide-sidebar-insights", false);
				bool hideSidebarLiveVal = AppState::configManager.getConfig<bool>("hide-sidebar-live", false);
				bool hideSidebarExtensionsVal =
					AppState::configManager.getConfig<bool>("hide-sidebar-extensions", false);
				bool hideSidebarKofiVal = AppState::configManager.getConfig<bool>("hide-sidebar-kofi", false);
				bool hideSidebarFeedbackVal = AppState::configManager.getConfig<bool>("hide-sidebar-feedback", false);
				bool hideSidebarAboutVal = AppState::configManager.getConfig<bool>("hide-sidebar-about", false);
				bool hideSidebarThemesVal = AppState::configManager.getConfig<bool>("hide-sidebar-themes", false);
				bool sidebarAlwaysExpandedVal =
					AppState::configManager.getConfig<bool>("sidebar-always-expanded", false);
				std::string sidebarPositionVal =
					AppState::configManager.getConfig<std::string>("sidebar-position", "left");
				std::string sidebarTypeVal =
					AppState::configManager.getConfig<std::string>("sidebar-type", "floating");
				float dataViewCardHeightVal =
					AppState::configManager.getConfig<float>("dataview-top-card-height", 255.0f);
				float projectViewCardHeightVal =
					AppState::configManager.getConfig<float>("projectview-card-height", 842.0f);
				float tabViewCardHeightVal = AppState::configManager.getConfig<float>("tabview-card-height", 848.0f);

				bool trackAppsVal;
				bool trackBrowserVal;
				bool trackProjectsVal;
				{
					std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
					trackAppsVal = AppState::state.trackApp;
					trackBrowserVal = AppState::state.trackTab;
					trackProjectsVal = AppState::state.trackProject;
				}

				auto getProp = [&](const char *name) { return (*handle)->get_property(name); };

				auto setPropIfChanged = [&](const char *name, const slint::interpreter::Value &val)
				{
					if (auto current = getProp(name))
					{
						if (*current != val)
						{
							(*handle)->set_property(name, val);
						}
					}
				};

				setPropIfChanged("themeMode_S", slint::interpreter::Value(slint::SharedString(theme)));
				setPropIfChanged("isDarkMode", slint::interpreter::Value(isDark));
				setPropIfChanged("autostart_S", slint::interpreter::Value(autostartVal));
				setPropIfChanged("headless_S", slint::interpreter::Value(headlessVal));
				setPropIfChanged("trueHeadless_S", slint::interpreter::Value(trueHeadlessVal));
				setPropIfChanged("telemetry_S", slint::interpreter::Value(telemetryVal));
				setPropIfChanged("showMiscImagePanel_S", slint::interpreter::Value(showMiscImageVal));
				setPropIfChanged("no_title_bar_enabled", slint::interpreter::Value(noTitleBarVal));
				setPropIfChanged("cornerRoundness_S", slint::interpreter::Value((double)cornerRoundnessVal));
				setPropIfChanged("uiScale_S", slint::interpreter::Value((double)uiScaleVal));
				setPropIfChanged("windowName_S", slint::interpreter::Value(slint::SharedString(currentWindowName)));
				setPropIfChanged("trackedTime_S", slint::interpreter::Value((double)totalTrackedTime));
				setPropIfChanged("trackedTime_Tab_S", slint::interpreter::Value((double)totalTrackedTime_Tab));
				setPropIfChanged("trackedTime_Project_S", slint::interpreter::Value((double)totalTrackedTime_Project));

				setPropIfChanged("killApps_S", slint::interpreter::Value(killAppsVal));
				setPropIfChanged("hardwareAccel_S", slint::interpreter::Value(hwAccelVal));
				setPropIfChanged("allowCustomBackends_S", slint::interpreter::Value(allowCustomVal));
				setPropIfChanged("allowNetworkActivity_S", slint::interpreter::Value(allowNetworkVal));
				setPropIfChanged("allowUpdateChecking_S", slint::interpreter::Value(allowUpdateCheckingVal));
				setPropIfChanged("updateAvailable_S", slint::interpreter::Value(updateAvailableVal));
				setPropIfChanged("isUpdating_S", slint::interpreter::Value(isUpdatingVal));
				setPropIfChanged("updateProgress_S", slint::interpreter::Value((double)updateProgressVal));
				setPropIfChanged("updateStatusText_S", slint::interpreter::Value(slint::SharedString(updateStatusTextVal)));
					setPropIfChanged("showContinentPrompt_S", slint::interpreter::Value(showContinentPromptVal));
				setPropIfChanged("allowNativeLibraries_S", slint::interpreter::Value(allowNativeLibrariesVal));
				setPropIfChanged("fullBrightnessBars_S", slint::interpreter::Value(fullBrightnessBarsVal));

				setPropIfChanged("allowSidebarCustomization_S",
								 slint::interpreter::Value(allowSidebarCustomizationVal));
				setPropIfChanged("hideSidebarReload_S", slint::interpreter::Value(hideSidebarReloadVal));
				setPropIfChanged("hideSidebarLimits_S", slint::interpreter::Value(hideSidebarLimitsVal));
				setPropIfChanged("hideSidebarHistory_S", slint::interpreter::Value(hideSidebarHistoryVal));
				setPropIfChanged("hideSidebarTimeline_S", slint::interpreter::Value(hideSidebarTimelineVal));
				setPropIfChanged("hideSidebarTab_S", slint::interpreter::Value(hideSidebarTabVal));
				setPropIfChanged("hideSidebarProject_S", slint::interpreter::Value(hideSidebarProjectVal));
				setPropIfChanged("hideSidebarInsights_S", slint::interpreter::Value(hideSidebarInsightsVal));
				setPropIfChanged("hideSidebarLive_S", slint::interpreter::Value(hideSidebarLiveVal));
				setPropIfChanged("hideSidebarExtensions_S", slint::interpreter::Value(hideSidebarExtensionsVal));
				setPropIfChanged("hideSidebarKofi_S", slint::interpreter::Value(hideSidebarKofiVal));
				setPropIfChanged("hideSidebarFeedback_S", slint::interpreter::Value(hideSidebarFeedbackVal));
				setPropIfChanged("hideSidebarAbout_S", slint::interpreter::Value(hideSidebarAboutVal));
				setPropIfChanged("hideSidebarThemes_S", slint::interpreter::Value(hideSidebarThemesVal));
				setPropIfChanged("sidebarAlwaysExpanded_S", slint::interpreter::Value(sidebarAlwaysExpandedVal));
				setPropIfChanged("sidebarPosition_S",
								 slint::interpreter::Value(slint::SharedString(sidebarPositionVal)));
				setPropIfChanged("sidebarType_S",
								 slint::interpreter::Value(slint::SharedString(sidebarTypeVal)));
				setPropIfChanged("trackApps_S", slint::interpreter::Value(trackAppsVal));
				setPropIfChanged("trackBrowser_S", slint::interpreter::Value(trackBrowserVal));
				setPropIfChanged("trackProjects_S", slint::interpreter::Value(trackProjectsVal));
				setPropIfChanged("dataViewCardHeight_S", slint::interpreter::Value(dataViewCardHeightVal));
				setPropIfChanged("projectViewCardHeight_S", slint::interpreter::Value(projectViewCardHeightVal));
				setPropIfChanged("tabViewCardHeight_S", slint::interpreter::Value(tabViewCardHeightVal));

				auto syncModel = [](std::shared_ptr<slint::VectorModel<slint::interpreter::Value>> model,
									const std::vector<slint::interpreter::Value> &vec)
				{
					size_t existing_count = model->row_count();
					size_t new_count = vec.size();
					size_t min_count = (std::min)(existing_count, new_count);

					// Update existing rows only if they changed
					for (size_t i = 0; i < min_count; ++i)
					{
						if (auto existing = model->row_data(i))
						{
							if (*existing == vec[i])
							{
								continue;
							}
						}
						model->set_row_data(i, vec[i]);
					}

					// Remove excess rows
					while (model->row_count() > new_count)
					{
						model->erase(model->row_count() - 1);
					}

					// Add new rows
					for (size_t i = existing_count; i < new_count; ++i)
					{
						model->push_back(vec[i]);
					}
				};

				syncModel(timeLogModel_interp, slintVec_TimeLog);
				syncModel(timeLogModelTab_interp, slintVec_TimeLog_Tab);
				syncModel(timeLogModelProject_interp, slintVec_TimeLog_Project);
				syncModel(switchHistoryModel_interp, slintVec_SwitchHistory);
				syncModel(rawAppsModel_interp, slintVec_RawApps);

				// Reconstruct and sync timeline events (interpreted)
				int preset = (*handle)
								 ->get_property("timelinePresetHours")
								 .value_or(slint::interpreter::Value(24.0))
								 .to_number()
								 .value_or(24.0);
				int startH = (*handle)
								 ->get_property("timelineStartHour")
								 .value_or(slint::interpreter::Value(0.0))
								 .to_number()
								 .value_or(0.0);
				int endH = (*handle)
							   ->get_property("timelineEndHour")
							   .value_or(slint::interpreter::Value(24.0))
							   .to_number()
							   .value_or(24.0);

				static int lastPreset = -1;
				static int lastStartH = -1;
				static int lastEndH = -1;
				static auto lastTimelineUpdate = std::chrono::steady_clock::now();

				auto now = std::chrono::steady_clock::now();
				bool valuesChanged = (preset != lastPreset || startH != lastStartH || endH != lastEndH);
				bool timeElapsed =
					(std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTimelineUpdate).count() >= 1000);

				if (valuesChanged || timeElapsed)
				{
					TimelineManager::updateTimeline(preset, startH, endH);
					lastPreset = preset;
					lastStartH = startH;
					lastEndH = endH;
					lastTimelineUpdate = now;
				}

				auto getAppColor = [](const std::string &appName) -> slint::Color
				{
					float h = static_cast<float>((appName.length() * 47) % 360);
					return slint::Color::from_hsva(h, 0.75f, 0.85f, 1.0f);
				};

				std::vector<slint::interpreter::Value> slintVec_Timeline;
				std::vector<slint::interpreter::Value> slintVec_TimelineMarkers;
				{
					std::lock_guard<std::mutex> lock(AppState::timelineMutex);
					for (const auto &item : AppState::timelineEvents)
					{
						slint::interpreter::Struct entry;
						entry.set_field("x", slint::interpreter::Value(item.x));
						entry.set_field("width", slint::interpreter::Value(item.width));
						entry.set_field("color", slint::interpreter::Value(getAppColor(item.appName)));
						entry.set_field("appName", slint::interpreter::Value(slint::SharedString(item.appName)));
						entry.set_field("duration", slint::interpreter::Value(slint::SharedString(item.duration)));
						entry.set_field("timeRange", slint::interpreter::Value(slint::SharedString(item.timeRange)));
						slintVec_Timeline.push_back(slint::interpreter::Value(entry));
					}
					for (const auto &m : AppState::timelineMarkers)
					{
						slint::interpreter::Struct entry;
						entry.set_field("x", slint::interpreter::Value(m.x));
						entry.set_field("label", slint::interpreter::Value(slint::SharedString(m.label)));
						slintVec_TimelineMarkers.push_back(slint::interpreter::Value(entry));
					}
				}
				syncModel(timelineBlocksModel_interp, slintVec_Timeline);
				syncModel(timelineMarkersModel_interp, slintVec_TimelineMarkers);

				// Sync theme configuration details (interpreted only)
				bool useThemes = AppState::configManager.getConfig("use-interpreter", false);
				(*handle)->set_property("useThemesEnabled_S", slint::interpreter::Value(useThemes));
				(*handle)->set_property("hprVersion_S",
										slint::interpreter::Value(slint::SharedString(AppState::APP_VERSION)));
				(*handle)->set_property("themesAvailable_S",
										slint::interpreter::Value(AppState::themeManager.areThemesAvailable));
				(*handle)->set_property("currentTheme_S",
										slint::interpreter::Value(slint::SharedString(currentSelectedTheme)));

				std::string activeTheme = AppState::configManager.getConfig("custom-theme", std::string("Default"));
				(*handle)->set_property("activeTheme_S", slint::interpreter::Value(slint::SharedString(activeTheme)));

				// Sync themes list to drop-down model
				std::vector<slint::interpreter::Value> slintVec_ThemesList;
				slintVec_ThemesList.push_back(slint::interpreter::Value(slint::SharedString("Default")));
				for (const auto &[key, path] : AppState::themeManager.availableThemes)
				{
					slint::interpreter::Value themeNameVal(slint::SharedString(key.first));
					bool found = false;
					for (const auto &val : slintVec_ThemesList)
					{
						if (std::string(val.to_string().value_or("")) == key.first)
						{
							found = true;
							break;
						}
					}
					if (!found)
					{
						slintVec_ThemesList.push_back(themeNameVal);
					}
				}
				syncModel(themesListModel_interp, slintVec_ThemesList);

				// Sync currently selected theme's compatibility version and author
				std::string themeVersionStr = (currentSelectedTheme == "Default") ? std::string(AppState::APP_VERSION) : "";
				std::string activeVersionStr = (activeTheme == "Default") ? std::string(AppState::APP_VERSION) : "";
				for (const auto &[key, path] : AppState::themeManager.availableThemes)
				{
					if (key.first == currentSelectedTheme)
					{
						themeVersionStr = key.second;
					}
					if (key.first == activeTheme)
					{
						activeVersionStr = key.second;
					}
				}
				(*handle)->set_property("themeVersion_S",
										slint::interpreter::Value(slint::SharedString(themeVersionStr)));
				(*handle)->set_property("activeThemeVersion_S",
										slint::interpreter::Value(slint::SharedString(activeVersionStr)));

				std::string currentAuthor = AppState::themeManager.availableThemes_Author.contains(currentSelectedTheme)
												? AppState::themeManager.availableThemes_Author[currentSelectedTheme]
												: "Unknown";
				std::string activeAuthor = AppState::themeManager.availableThemes_Author.contains(activeTheme)
											   ? AppState::themeManager.availableThemes_Author[activeTheme]
											   : "Unknown";
				(*handle)->set_property("currentThemeAuthor_S",
										slint::interpreter::Value(slint::SharedString(currentAuthor)));
				(*handle)->set_property("activeThemeAuthor_S",
										slint::interpreter::Value(slint::SharedString(activeAuthor)));
				(*handle)->set_property("themeAuthor_S",
										slint::interpreter::Value(slint::SharedString(currentAuthor)));

				// Check if themes tab is open in Slint UI to unload preview
				// images when not in use
				std::string activeView = "";
				if (auto optActive = (*handle)->get_property("active-view"))
				{
					if (auto strVal = optActive->to_string())
					{
						activeView = std::string(*strVal);
					}
				}

				if (activeView != "THEMES")
				{
					// Unload theme preview images from memory when user is not
					// viewing the themes page
					if (themePreviewsModel_interp->row_count() > 0)
					{
						themePreviewsModel_interp->clear();
						(*handle)->set_property("themePreviewImageCount_S", slint::interpreter::Value(0.0));
						if (lastLoadedPreviewThemePtr)
						{
							*lastLoadedPreviewThemePtr = ""; // force reload when navigating back
						}
					}
				}
				else
				{
					// If on themes page, sync previews only when theme changes
					// or was cleared
					bool themeChanged =
						(lastLoadedPreviewThemePtr && *lastLoadedPreviewThemePtr != currentSelectedTheme);
					if (themeChanged || themePreviewsModel_interp->row_count() == 0)
					{
						if (lastLoadedPreviewThemePtr)
						{
							*lastLoadedPreviewThemePtr = currentSelectedTheme;
						}
						std::vector<slint::interpreter::Value> slintVec_Previews;
						if (AppState::themeManager.themePreview.contains(currentSelectedTheme))
						{
							for (const auto &path : AppState::themeManager.themePreview[currentSelectedTheme])
							{
								auto img = slint::Image::load_from_path(slint::SharedString(path));
								slintVec_Previews.push_back(slint::interpreter::Value(img));
							}
						}
						syncModel(themePreviewsModel_interp, slintVec_Previews);
						(*handle)->set_property(
							"themePreviewImageCount_S",
							slint::interpreter::Value(static_cast<double>(slintVec_Previews.size())));
					}
				}
			}
		});
}

/*
	Updates the interpreted Ui's Insights data
*/
void UiModelManager::showInsights_Interpreted(const std::string score,
											  const std::string mostUsed, const std::string totalTrackedTime,
											  const std::string switchCount, const std::string mostSwitchedFrom,
											  const std::string mostSwitchedTo, const std::string mostFocusedSession,
											  const std::string mostProductiveHour, const std::string escapePattern,
											  const std::string returnRate, const std::string avgFocusSession,
											  const std::string mostDistractedDay, const std::string productiveDays,
											  const std::string screenTimeVsAvg, const std::string focusDipHour,
											  const std::string deepWorkBeforeNoon, const std::string weekendVsWeekday)
{
	slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> weak(ui_interp.value());
	slint::invoke_from_event_loop(
		[weak, score, mostUsed, totalTrackedTime, switchCount, mostSwitchedFrom, mostSwitchedTo, mostFocusedSession,
		 mostProductiveHour, escapePattern, returnRate, avgFocusSession, mostDistractedDay, productiveDays,
		 screenTimeVsAvg, focusDipHour, deepWorkBeforeNoon, weekendVsWeekday, this]()
		{
			if (auto handle = weak.lock())
			{
				auto getProp = [&](const char *name) { return (*handle)->get_property(name); };

				auto setPropIfChanged = [&](const char *name, const slint::interpreter::Value &val)
				{
					if (auto current = getProp(name))
					{
						if (*current == val)
						{
							return;
						}
					}
					(*handle)->set_property(name, val);
				};

				setPropIfChanged("productivityScore_S", slint::interpreter::Value(slint::SharedString(score)));
				setPropIfChanged("mostUsedApp_S", slint::interpreter::Value(slint::SharedString(mostUsed)));
				setPropIfChanged("totalTrackedTime_S",
								 slint::interpreter::Value(slint::SharedString(totalTrackedTime)));
				setPropIfChanged("totalSwitches_S", slint::interpreter::Value(slint::SharedString(switchCount)));
				setPropIfChanged("mostSwitchedFrom_S",
								 slint::interpreter::Value(slint::SharedString(mostSwitchedFrom)));
				setPropIfChanged("mostSwitchedTo_S", slint::interpreter::Value(slint::SharedString(mostSwitchedTo)));
				setPropIfChanged("longestFocus_S", slint::interpreter::Value(slint::SharedString(mostFocusedSession)));
				setPropIfChanged("peakHour_S", slint::interpreter::Value(slint::SharedString(mostProductiveHour)));
				setPropIfChanged("escapePattern_S", slint::interpreter::Value(slint::SharedString(escapePattern)));
				setPropIfChanged("returnRate_S", slint::interpreter::Value(slint::SharedString(returnRate)));
				setPropIfChanged("avgFocusSession_S", slint::interpreter::Value(slint::SharedString(avgFocusSession)));
				setPropIfChanged("mostDistractedDay_S",
								 slint::interpreter::Value(slint::SharedString(mostDistractedDay)));
				setPropIfChanged("productiveDays_S", slint::interpreter::Value(slint::SharedString(productiveDays)));
				setPropIfChanged("screenTimeVsAvg_S", slint::interpreter::Value(slint::SharedString(screenTimeVsAvg)));
				setPropIfChanged("focusDipHour_S", slint::interpreter::Value(slint::SharedString(focusDipHour)));
				setPropIfChanged("deepWorkBeforeNoon_S",
								 slint::interpreter::Value(slint::SharedString(deepWorkBeforeNoon)));
				setPropIfChanged("weekendVsWeekday_S",
								 slint::interpreter::Value(slint::SharedString(weekendVsWeekday)));
			}
		});
}

/*
	Updates the interpreted Ui's extensions data
*/
void UiModelManager::showExtensions_Interpreted(const std::map<std::pair<std::string, std::string>, AppState::ExtensionInfo> &extensionsMap)
{
	if (extensionsMap == lastKnownExtensionsMap)
		return;
	lastKnownExtensionsMap = extensionsMap;

	// Detect HPR Store: author == "plexescor" && name == "hpr store" (case-insensitive)
	auto toLower = [](std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
		return s;
	};
	bool hprStoreInstalled = false;
	for (const auto &[key, info] : extensionsMap)
	{
		if (toLower(info.author) == "plexescor" && toLower(info.name) == "hpr store")
		{
			hprStoreInstalled = true;
			break;
		}
	}

	std::vector<slint::interpreter::Value> vec;
	for (const auto &[key, info] : extensionsMap)
	{
		slint::interpreter::Struct entry;
		entry.set_field("author-name", slint::interpreter::Value(slint::SharedString(info.author)));
		entry.set_field("extension-name", slint::interpreter::Value(slint::SharedString(info.name)));
		entry.set_field("is-compatible", slint::interpreter::Value(info.isCompatible));
		entry.set_field("compatibility-warning", slint::interpreter::Value(slint::SharedString(info.warningMessage)));
		vec.push_back(slint::interpreter::Value(entry));
	}

	slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> weak(ui_interp.value());
	slint::invoke_from_event_loop(
		[weak, vec, hprStoreInstalled, this]()
		{
			if (auto handle = weak.lock())
			{
				extensionsModel_interp->clear();
				for (const auto &e : vec)
					extensionsModel_interp->push_back(e);
				if ((*handle)->get_property("hprStoreInstalled_S").has_value())
					(*handle)->set_property("hprStoreInstalled_S", slint::interpreter::Value(hprStoreInstalled));
			}
		});
}

/*
	Updates the interpreteds Ui's Fun Stats such as (CPU Usage, RAM Usage etc) by HPR
*/
void UiModelManager::showFunStats_Interpreted(const std::string &cpu, const std::string &ram, const std::string &ext,
											  const std::string &threads)
{
	if (!ui_interp.has_value())
		return;
	slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> weak(ui_interp.value());
	slint::invoke_from_event_loop(
		[weak, cpu, ram, ext, threads]()
		{
			if (auto handle = weak.lock())
			{
				auto getProp = [&](const char *name) { return (*handle)->get_property(name); };

				auto setPropIfChanged = [&](const char *name, const slint::interpreter::Value &val)
				{
					if (auto current = getProp(name))
					{
						if (*current == val)
						{
							return;
						}
					}
					(*handle)->set_property(name, val);
				};

				setPropIfChanged("cpuUsage", slint::interpreter::Value(slint::SharedString(cpu)));
				setPropIfChanged("ramUsage", slint::interpreter::Value(slint::SharedString(ram)));
				setPropIfChanged("loadedExtensions", slint::interpreter::Value(slint::SharedString(ext)));
				setPropIfChanged("activeThreads", slint::interpreter::Value(slint::SharedString(threads)));
			}
		});
}