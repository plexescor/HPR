/*
	Why is all the code for pushing the data to the ui handled in UiModelManager alone?
	Because it then isolates the code which touches the ui and why do we want isolation?
	For my future idea, make the entire ui changable, like using Qt, Electron, Tauri(<-maybe)
	So this isolation into one place or you can say class makes it easy to narrow down what object to create or not
	In this case, if lua registred an override, then we can just not instantiate the UiModelManager
	and UiEventBridge at all... and not run our even loop, the Ui made via the dll/so loaded by lua will handle the loop itself
	⚠️⚠️⚠️⚠️⚠️⚠️⚠️ Thing to keep in mind is that this interceptable Ui is still just a concept
	but what bad happens if we still implement this isolation? Apart from messy and bad code, i dont think theres any other 
	disadvantage, thanks for bearing with me.
*/

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
#include "app-window.h"
#include <slint.h>

UiModelManager::UiModelManager(slint::ComponentHandle<MainWindow> &ui_handle) : ui(ui_handle)
{
	this->ui = ui_handle;
	// make shared so they aint null and no thing crashes
	timeLogModel = std::make_shared<slint::VectorModel<TimeLog>>();
	timeLogModelTab = std::make_shared<slint::VectorModel<TimeLog_Tab>>();
	timeLogModelProject = std::make_shared<slint::VectorModel<TimeLog_Project>>();
	switchHistoryModel = std::make_shared<slint::VectorModel<SwitchHistory>>();
	extensionsModel = std::make_shared<slint::VectorModel<LoadedExtension_S>>();
	rawAppsModel = std::make_shared<slint::VectorModel<AppGoalData>>();
	timelineBlocksModel = std::make_shared<slint::VectorModel<TimelineBlock>>();
	timelineMarkersModel = std::make_shared<slint::VectorModel<TimelineMarker>>();

	slint::ComponentWeakHandle<MainWindow> weak(ui.value());
	slint::invoke_from_event_loop(
		[weak, this]()
		{
			if (auto handle = weak.lock())
			{
				// set in ui
				(*handle)->set_timePerApp_S(timeLogModel);
				(*handle)->set_timePerTab_S(timeLogModelTab);
				(*handle)->set_timePerProject_S(timeLogModelProject);
				(*handle)->set_switchHistory_S(switchHistoryModel);
				(*handle)->set_loadedExtensions_S(extensionsModel);
				(*handle)->set_rawApps_S(rawAppsModel);
				(*handle)->set_timelineBlocks_S(timelineBlocksModel);
				(*handle)->set_timelineMarkers_S(timelineMarkersModel);
			}
		});
}

UiModelManager::~UiModelManager() {}

/*
	Updates the compiled Ui's core data
*/
void UiModelManager::update(const std::map<std::string, uint64_t> &rawTimeLog,
							const std::map<std::string, uint64_t> &rawTimeLog_Tab,
							const std::map<std::string, uint64_t> &rawTimeLog_Project,
							const std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> &rawHistory,
							std::string &rawCurrentWindowName, uint64_t &totalTrackedTime,
							uint64_t &totalTrackedTime_Tab, uint64_t &totalTrackedTime_Project,
							AliasManager &aliasManager)
{

	std::string currentWindowName = aliasManager.getAlias(rawCurrentWindowName);
	//----------------------TIME
	// LOG-----------------------------------------------

	// make a middle man translatedTimeLog with correct aliases and push
	// rawTimeLog
	std::map<std::string, uint64_t> translatedTimeLog;

	for (const auto &[raw, duration] : rawTimeLog)
	{
		translatedTimeLog[aliasManager.getAlias(raw)] += duration;
		totalTrackedTime += duration;
	}

	// TAB
	bool isTabView;
	std::map<std::string, uint64_t> translatedTimeLog_Tab;
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
	std::map<std::string, uint64_t> translatedTimeLog_Project;
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
			// – file [module]" or just "ProjectName" fragile as fuck
			else if (raw.contains("jetbrains: "))
			{
				;
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

	// create a vector of slint's TimeLog struct
	// Use pretty names
	std::vector<TimeLog> slintVec_TimeLog;
	for (const auto &[name, duration] : translatedTimeLog)
	{
		slintVec_TimeLog.push_back({slint::SharedString(name),
									slint::SharedString(formatTime_HHMMSS(duration)), // The HH:MM:SS string
									(float)duration});
	}

	std::vector<TimeLog_Tab> slintVec_TimeLog_Tab;
	for (const auto &[name, duration] : translatedTimeLog_Tab)
	{
		slintVec_TimeLog_Tab.push_back({slint::SharedString(name),
										slint::SharedString(formatTime_HHMMSS(duration)), // The HH:MM:SS string
										(float)duration});
	}

	std::vector<TimeLog_Project> slintVec_TimeLog_Project;
	for (const auto &[name, duration] : translatedTimeLog_Project)
	{
		slintVec_TimeLog_Project.push_back({slint::SharedString(name),
											slint::SharedString(formatTime_HHMMSS(duration)), // The HH:MM:SS string
											(float)duration});
	}

	// Sort so most used comes at top
	std::sort(slintVec_TimeLog.begin(), slintVec_TimeLog.end(),
			  [](const TimeLog &a, const TimeLog &b) { return a.duration_i > b.duration_i; });

	std::sort(slintVec_TimeLog_Tab.begin(), slintVec_TimeLog_Tab.end(),
			  [](const TimeLog_Tab &a, const TimeLog_Tab &b) { return a.duration_i > b.duration_i; });

	std::sort(slintVec_TimeLog_Project.begin(), slintVec_TimeLog_Project.end(),
			  [](const TimeLog_Project &a, const TimeLog_Project &b) { return a.duration_i > b.duration_i; });

	// Raw Apps extraction from rawTimeLog with Limits & Goals from AppState
	std::vector<AppGoalData> slintVec_RawApps;
	{
		std::map<std::string, int> limits;
		std::map<std::string, int> goals;

		{
			std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);

			limits = AppState::state.appLimits;
			goals = AppState::state.appGoals;
		}
		for (const auto &[raw, duration] : rawTimeLog)
		{
			int limit = 0;
			int goal = 0;

			if (limits.count(raw))
				limit = limits.at(raw);

			if (goals.count(raw))
				goal = goals.at(raw);

			std::string limit_rem;
			std::string goal_rem;
			{
				std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
				if (AppState::limitsManager)
				{
					limit_rem = AppState::limitsManager->getLimitRemaining(raw, duration, limit);
					goal_rem = AppState::limitsManager->getGoalRemaining(raw, duration, goal);
				}
			}

			// get display name using aliasManager under lua lock or default
			std::string displayName;
			{
				// aliasManager uses extensions (lua state), so lock luaMutex as
				// well if it exists or use stateMutex
				std::lock_guard<std::recursive_mutex> lock(AppState::stateMutex);
				displayName = aliasManager.getAlias(raw);
			}

			slintVec_RawApps.push_back({slint::SharedString(displayName), slint::SharedString(raw), limit, goal,
										slint::SharedString(limit_rem), slint::SharedString(goal_rem)});
		}
	}

	//----------------------------------SWITCH
	// HISTORY---------------------------------------

	// Middle man
	// From Window, to window, timestamp in str
	// std::map<std::pair<std::string, std::string>, std::string>
	// translatedSwitchHistory;

	// For max value specifically
	struct TempSwitchHistory
	{
		std::string from;
		std::string to;
		uint64_t maxVal;
	};

	std::vector<TempSwitchHistory> tempSwitchVec;
	tempSwitchVec.reserve(rawHistory.size());

	for (const auto &[route, timeStampS] : rawHistory)
	{
		if (timeStampS.empty())
			continue; // Safety guard

		const auto &[from, to] = route;

		const auto maxVal = *std::max_element(timeStampS.begin(), timeStampS.end());
		tempSwitchVec.push_back({from, to, maxVal});
	}

	// sort so latest one comes at top
	std::sort(tempSwitchVec.begin(), tempSwitchVec.end(),
			  [](const TempSwitchHistory &a, const TempSwitchHistory &b) { return a.maxVal > b.maxVal; });

	// create a vector of slint's switch History struct
	// Use pretty names
	std::vector<SwitchHistory> slintVec_SwitchHistory;
	size_t switchHistoryLimit = std::min(tempSwitchVec.size(), size_t(100));

	for (size_t i = 0; i < switchHistoryLimit; ++i)
	{
		const auto &item = tempSwitchVec[i];
		slintVec_SwitchHistory.push_back(SwitchHistory{slint::SharedString(aliasManager.getAlias(item.from)),
													   slint::SharedString(aliasManager.getAlias(item.to)),
													   slint::SharedString(convertToTime_HHMMSS_12(item.maxVal))});
	}

	//-----------------Actually pushing changes to slint for
	// rendering-----------------------
	if (!ui.has_value())
	{
		return; // skip update if ui handle aint ready
	}

	std::string theme = AppState::configManager.getConfig<std::string>("theme", "dark");
	bool isDark = (theme != "light");

	slint::ComponentWeakHandle<MainWindow> weak(ui.value());
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

				if ((*handle)->get_themeMode_S() != slint::SharedString(theme))
					(*handle)->set_themeMode_S(slint::SharedString(theme));
				if ((*handle)->get_isDarkMode() != isDark)
					(*handle)->set_isDarkMode(isDark);
				if ((*handle)->get_autostart_S() != autostartVal)
					(*handle)->set_autostart_S(autostartVal);
				if ((*handle)->get_headless_S() != headlessVal)
					(*handle)->set_headless_S(headlessVal);
				if ((*handle)->get_trueHeadless_S() != trueHeadlessVal)
					(*handle)->set_trueHeadless_S(trueHeadlessVal);
				if ((*handle)->get_telemetry_S() != telemetryVal)
					(*handle)->set_telemetry_S(telemetryVal);
				if ((*handle)->get_showMiscImagePanel_S() != showMiscImageVal)
					(*handle)->set_showMiscImagePanel_S(showMiscImageVal);
				if ((*handle)->get_no_title_bar_enabled() != noTitleBarVal)
					(*handle)->set_no_title_bar_enabled(noTitleBarVal);
				if ((*handle)->get_cornerRoundness_S() != cornerRoundnessVal)
					(*handle)->set_cornerRoundness_S(cornerRoundnessVal);
				if ((*handle)->get_uiScale_S() != uiScaleVal)
					(*handle)->set_uiScale_S(uiScaleVal);
				if ((*handle)->get_windowName_S() != slint::SharedString(currentWindowName))
					(*handle)->set_windowName_S(slint::SharedString(currentWindowName));

				if ((*handle)->get_trackedTime_S() != (float)totalTrackedTime)
					(*handle)->set_trackedTime_S((float)totalTrackedTime);
				if ((*handle)->get_trackedTime_Tab_S() != (float)totalTrackedTime_Tab)
					(*handle)->set_trackedTime_Tab_S((float)totalTrackedTime_Tab);
				if ((*handle)->get_trackedTime_Project_S() != (float)totalTrackedTime_Project)
					(*handle)->set_trackedTime_Project_S((float)totalTrackedTime_Project);

				if ((*handle)->get_killApps_S() != killAppsVal)
					(*handle)->set_killApps_S(killAppsVal);
				if ((*handle)->get_hardwareAccel_S() != hwAccelVal)
					(*handle)->set_hardwareAccel_S(hwAccelVal);
				if ((*handle)->get_allowCustomBackends_S() != allowCustomVal)
					(*handle)->set_allowCustomBackends_S(allowCustomVal);
				if ((*handle)->get_allowNetworkActivity_S() != allowNetworkVal)
					(*handle)->set_allowNetworkActivity_S(allowNetworkVal);
				if ((*handle)->get_allowUpdateChecking_S() != allowUpdateCheckingVal)
					(*handle)->set_allowUpdateChecking_S(allowUpdateCheckingVal);
				if ((*handle)->get_updateAvailable_S() != updateAvailableVal)
					(*handle)->set_updateAvailable_S(updateAvailableVal);
				if ((*handle)->get_isUpdating_S() != isUpdatingVal)
					(*handle)->set_isUpdating_S(isUpdatingVal);
				if ((*handle)->get_updateProgress_S() != updateProgressVal)
					(*handle)->set_updateProgress_S(updateProgressVal);
				if ((*handle)->get_updateStatusText_S() != slint::SharedString(updateStatusTextVal))
					(*handle)->set_updateStatusText_S(slint::SharedString(updateStatusTextVal));
					if ((*handle)->get_showContinentPrompt_S() != showContinentPromptVal)
					(*handle)->set_showContinentPrompt_S(showContinentPromptVal);
				if ((*handle)->get_allowNativeLibraries_S() != allowNativeLibrariesVal)
					(*handle)->set_allowNativeLibraries_S(allowNativeLibrariesVal);
				if ((*handle)->get_fullBrightnessBars_S() != fullBrightnessBarsVal)
					(*handle)->set_fullBrightnessBars_S(fullBrightnessBarsVal);

				if ((*handle)->get_allowSidebarCustomization_S() != allowSidebarCustomizationVal)
					(*handle)->set_allowSidebarCustomization_S(allowSidebarCustomizationVal);
				if ((*handle)->get_hideSidebarReload_S() != hideSidebarReloadVal)
					(*handle)->set_hideSidebarReload_S(hideSidebarReloadVal);
				if ((*handle)->get_hideSidebarLimits_S() != hideSidebarLimitsVal)
					(*handle)->set_hideSidebarLimits_S(hideSidebarLimitsVal);
				if ((*handle)->get_hideSidebarHistory_S() != hideSidebarHistoryVal)
					(*handle)->set_hideSidebarHistory_S(hideSidebarHistoryVal);
				if ((*handle)->get_hideSidebarTimeline_S() != hideSidebarTimelineVal)
					(*handle)->set_hideSidebarTimeline_S(hideSidebarTimelineVal);
				if ((*handle)->get_hideSidebarTab_S() != hideSidebarTabVal)
					(*handle)->set_hideSidebarTab_S(hideSidebarTabVal);
				if ((*handle)->get_hideSidebarProject_S() != hideSidebarProjectVal)
					(*handle)->set_hideSidebarProject_S(hideSidebarProjectVal);
				if ((*handle)->get_hideSidebarInsights_S() != hideSidebarInsightsVal)
					(*handle)->set_hideSidebarInsights_S(hideSidebarInsightsVal);
				if ((*handle)->get_hideSidebarLive_S() != hideSidebarLiveVal)
					(*handle)->set_hideSidebarLive_S(hideSidebarLiveVal);
				if ((*handle)->get_hideSidebarExtensions_S() != hideSidebarExtensionsVal)
					(*handle)->set_hideSidebarExtensions_S(hideSidebarExtensionsVal);
				if ((*handle)->get_hideSidebarKofi_S() != hideSidebarKofiVal)
					(*handle)->set_hideSidebarKofi_S(hideSidebarKofiVal);
				if ((*handle)->get_hideSidebarFeedback_S() != hideSidebarFeedbackVal)
					(*handle)->set_hideSidebarFeedback_S(hideSidebarFeedbackVal);
				if ((*handle)->get_hideSidebarAbout_S() != hideSidebarAboutVal)
					(*handle)->set_hideSidebarAbout_S(hideSidebarAboutVal);
				if ((*handle)->get_hideSidebarThemes_S() != hideSidebarThemesVal)
					(*handle)->set_hideSidebarThemes_S(hideSidebarThemesVal);
				if ((*handle)->get_sidebarAlwaysExpanded_S() != sidebarAlwaysExpandedVal)
					(*handle)->set_sidebarAlwaysExpanded_S(sidebarAlwaysExpandedVal);
				if ((*handle)->get_sidebarPosition_S() != slint::SharedString(sidebarPositionVal))
					(*handle)->set_sidebarPosition_S(slint::SharedString(sidebarPositionVal));
				if ((*handle)->get_sidebarType_S() != slint::SharedString(sidebarTypeVal))
					(*handle)->set_sidebarType_S(slint::SharedString(sidebarTypeVal));
				if ((*handle)->get_trackApps_S() != trackAppsVal)
					(*handle)->set_trackApps_S(trackAppsVal);
				if ((*handle)->get_trackBrowser_S() != trackBrowserVal)
					(*handle)->set_trackBrowser_S(trackBrowserVal);
				if ((*handle)->get_trackProjects_S() != trackProjectsVal)
					(*handle)->set_trackProjects_S(trackProjectsVal);
				if ((*handle)->get_dataViewCardHeight_S() != dataViewCardHeightVal)
					(*handle)->set_dataViewCardHeight_S(dataViewCardHeightVal);
				if ((*handle)->get_projectViewCardHeight_S() != projectViewCardHeightVal)
					(*handle)->set_projectViewCardHeight_S(projectViewCardHeightVal);
				if ((*handle)->get_tabViewCardHeight_S() != tabViewCardHeightVal)
					(*handle)->set_tabViewCardHeight_S(tabViewCardHeightVal);

				// Surgical update to prevent layout panics during
				// resize/maximize
				auto syncModel = [](auto model, const auto &vec)
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

					// Remove excess rows from the end
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

				syncModel(timeLogModel, slintVec_TimeLog);
				syncModel(timeLogModelTab, slintVec_TimeLog_Tab);
				syncModel(timeLogModelProject, slintVec_TimeLog_Project);
				syncModel(switchHistoryModel, slintVec_SwitchHistory);

				// Sync rawAppsModel
				auto syncModelRaw = [](auto model, const auto &vec)
				{
					size_t existing_count = model->row_count();
					size_t new_count = vec.size();
					size_t min_count = (std::min)(existing_count, new_count);

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

					while (model->row_count() > new_count)
					{
						model->erase(model->row_count() - 1);
					}

					for (size_t i = existing_count; i < new_count; ++i)
					{
						model->push_back(vec[i]);
					}
				};
				syncModelRaw(rawAppsModel, slintVec_RawApps);

				// Reconstruct and sync timeline events
				int preset = (*handle)->get_timelinePresetHours();
				int startH = (*handle)->get_timelineStartHour();
				int endH = (*handle)->get_timelineEndHour();

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

				std::vector<TimelineBlock> slintVec_Timeline;
				std::vector<TimelineMarker> slintVec_TimelineMarkers;
				{
					std::lock_guard<std::mutex> lock(AppState::timelineMutex);
					for (const auto &item : AppState::timelineEvents)
					{
						slintVec_Timeline.push_back(
							TimelineBlock{static_cast<float>(item.x), static_cast<float>(item.width),
										  getAppColor(item.appName), slint::SharedString(item.appName),
										  slint::SharedString(item.duration), slint::SharedString(item.timeRange)});
					}
					for (const auto &m : AppState::timelineMarkers)
					{
						slintVec_TimelineMarkers.push_back(
							TimelineMarker{static_cast<float>(m.x), slint::SharedString(m.label)});
					}
				}
				syncModel(timelineBlocksModel, slintVec_Timeline);
				syncModel(timelineMarkersModel, slintVec_TimelineMarkers);

				// Sync theme configuration details (compiled branch)
				bool useThemes = AppState::configManager.getConfig("use-interpreter", false);
				(*handle)->set_useThemesEnabled_S(useThemes);
				(*handle)->set_hprVersion_S(slint::SharedString(AppState::APP_VERSION));
				(*handle)->set_themesAvailable_S(AppState::themeManager.areThemesAvailable);
				(*handle)->set_currentTheme_S(slint::SharedString(currentSelectedTheme));

				std::string activeTheme = AppState::configManager.getConfig("custom-theme", std::string("Default"));
				(*handle)->set_activeTheme_S(slint::SharedString(activeTheme));

				std::string themeVersionStr = (currentSelectedTheme == "Default") ? std::string(AppState::APP_VERSION) : "";
				for (const auto &[key, path] : AppState::themeManager.availableThemes)
				{
					if (key.first == currentSelectedTheme)
					{
						themeVersionStr = key.second;
						break;
					}
				}
				(*handle)->set_themeVersion_S(slint::SharedString(themeVersionStr));

				std::string currentAuthor = AppState::themeManager.availableThemes_Author.contains(currentSelectedTheme)
												? AppState::themeManager.availableThemes_Author[currentSelectedTheme]
												: "Unknown";
				(*handle)->set_themeAuthor_S(slint::SharedString(currentAuthor));
			}
		});
}


/*
	Sets the currently selected theme to a std::string passed as a param by the caller
*/
void UiModelManager::setSelectedTheme(const std::string &themeName) { currentSelectedTheme = themeName; }

/*
	Updates the compiled Ui's Insights data
*/
void UiModelManager::showInsights(const std::string score,
								  const std::string mostUsed, const std::string totalTrackedTime,
								  const std::string switchCount, const std::string mostSwitchedFrom,
								  const std::string mostSwitchedTo, const std::string mostFocusedSession,
								  const std::string mostProductiveHour, const std::string escapePattern,
								  const std::string returnRate, const std::string avgFocusSession,
								  const std::string mostDistractedDay, const std::string productiveDays,
								  const std::string screenTimeVsAvg, const std::string focusDipHour,
								  const std::string deepWorkBeforeNoon, const std::string weekendVsWeekday)
{
	if (!ui.has_value())
	{
		return;
	}
	slint::ComponentWeakHandle<MainWindow> weak(ui.value());
	slint::invoke_from_event_loop(
		[weak, score, mostUsed, totalTrackedTime, switchCount, mostSwitchedFrom, mostSwitchedTo, mostFocusedSession,
		 mostProductiveHour, escapePattern, returnRate, avgFocusSession, mostDistractedDay, productiveDays,
		 screenTimeVsAvg, focusDipHour, deepWorkBeforeNoon, weekendVsWeekday, this]()
		{
			if (auto handle = weak.lock())
			{
				if ((*handle) -> get_productivityScore_S() != slint::SharedString(score))
					(*handle) -> set_productivityScore_S(slint::SharedString(score));
				if ((*handle)->get_mostUsedApp_S() != slint::SharedString(mostUsed))
					(*handle)->set_mostUsedApp_S(slint::SharedString(mostUsed));
				if ((*handle)->get_totalTrackedTime_S() != slint::SharedString(totalTrackedTime))
					(*handle)->set_totalTrackedTime_S(slint::SharedString(totalTrackedTime));
				if ((*handle)->get_totalSwitches_S() != slint::SharedString(switchCount))
					(*handle)->set_totalSwitches_S(slint::SharedString(switchCount));
				if ((*handle)->get_mostSwitchedFrom_S() != slint::SharedString(mostSwitchedFrom))
					(*handle)->set_mostSwitchedFrom_S(slint::SharedString(mostSwitchedFrom));
				if ((*handle)->get_mostSwitchedTo_S() != slint::SharedString(mostSwitchedTo))
					(*handle)->set_mostSwitchedTo_S(slint::SharedString(mostSwitchedTo));
				if ((*handle)->get_longestFocus_S() != slint::SharedString(mostFocusedSession))
					(*handle)->set_longestFocus_S(slint::SharedString(mostFocusedSession));
				if ((*handle)->get_peakHour_S() != slint::SharedString(mostProductiveHour))
					(*handle)->set_peakHour_S(slint::SharedString(mostProductiveHour));
				if ((*handle)->get_escapePattern_S() != slint::SharedString(escapePattern))
					(*handle)->set_escapePattern_S(slint::SharedString(escapePattern));
				if ((*handle)->get_returnRate_S() != slint::SharedString(returnRate))
					(*handle)->set_returnRate_S(slint::SharedString(returnRate));
				if ((*handle)->get_avgFocusSession_S() != slint::SharedString(avgFocusSession))
					(*handle)->set_avgFocusSession_S(slint::SharedString(avgFocusSession));
				if ((*handle)->get_mostDistractedDay_S() != slint::SharedString(mostDistractedDay))
					(*handle)->set_mostDistractedDay_S(slint::SharedString(mostDistractedDay));
				if ((*handle)->get_productiveDays_S() != slint::SharedString(productiveDays))
					(*handle)->set_productiveDays_S(slint::SharedString(productiveDays));
				if ((*handle)->get_screenTimeVsAvg_S() != slint::SharedString(screenTimeVsAvg))
					(*handle)->set_screenTimeVsAvg_S(slint::SharedString(screenTimeVsAvg));
				if ((*handle)->get_focusDipHour_S() != slint::SharedString(focusDipHour))
					(*handle)->set_focusDipHour_S(slint::SharedString(focusDipHour));
				if ((*handle)->get_deepWorkBeforeNoon_S() != slint::SharedString(deepWorkBeforeNoon))
					(*handle)->set_deepWorkBeforeNoon_S(slint::SharedString(deepWorkBeforeNoon));
				if ((*handle)->get_weekendVsWeekday_S() != slint::SharedString(weekendVsWeekday))
					(*handle)->set_weekendVsWeekday_S(slint::SharedString(weekendVsWeekday));
			}
		});
}

/*
	Updates the compiled Ui's extensions data
*/
void UiModelManager::showExtensions(const std::map<std::pair<std::string, std::string>, AppState::ExtensionInfo> &extensionsMap)
{
	if (extensionsMap == lastKnownExtensionsMap)
		return;
	lastKnownExtensionsMap = extensionsMap;

	if (!ui.has_value())
		return;

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

	std::vector<LoadedExtension_S> vec;
	for (const auto &[key, info] : extensionsMap)
	{
		vec.push_back({slint::SharedString(info.author), slint::SharedString(info.name), info.isCompatible,
					   slint::SharedString(info.warningMessage)});
	}

	slint::ComponentWeakHandle<MainWindow> weak(ui.value());
	slint::invoke_from_event_loop(
		[weak, vec, hprStoreInstalled, this]()
		{
			if (auto handle = weak.lock())
			{
				extensionsModel->clear();
				for (const auto &e : vec)
					extensionsModel->push_back(e);
				(*handle)->set_hprStoreInstalled_S(hprStoreInstalled);
			}
		});
}

/*
	Updates the compiled Ui's Fun Stats such as (CPU Usage, RAM Usage etc) by HPR
*/
void UiModelManager::showFunStats(const std::string &cpu, const std::string &ram, const std::string &ext,
								  const std::string &threads)
{
	if (!ui.has_value())
		return;
	slint::ComponentWeakHandle<MainWindow> weak(ui.value());
	slint::invoke_from_event_loop(
		[weak, cpu, ram, ext, threads]()
		{
			if (auto handle = weak.lock())
			{
				if ((*handle)->get_cpuUsage() != slint::SharedString(cpu))
					(*handle)->set_cpuUsage(slint::SharedString(cpu));
				if ((*handle)->get_ramUsage() != slint::SharedString(ram))
					(*handle)->set_ramUsage(slint::SharedString(ram));
				if ((*handle)->get_loadedExtensions() != slint::SharedString(ext))
					(*handle)->set_loadedExtensions(slint::SharedString(ext));
				if ((*handle)->get_activeThreads() != slint::SharedString(threads))
					(*handle)->set_activeThreads(slint::SharedString(threads));
			}
		});
}