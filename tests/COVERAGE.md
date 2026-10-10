# HPR Lua Extension API — Test Coverage

> Auto-derived from [`extensionManager.cpp`](../src/extension/extensionManager.cpp).
> Update this document whenever a new suite is added or a new API is registered.

## Summary

| Metric | Count |
|---|---|
| **Total API surface** | 98 |
| **Covered** | 36 |
| **Uncovered** | 62 |
| **Coverage** | 36.73% |

> **Note:** Lifecycle hooks (`init` / `onTick` / `onExit` / `onAction`) are invoked by the runtime directly and are not part of the 98 registered API functions, but are covered by the `lifecyclehooks` suite.

---

## ✅ Covered APIs

| API | Category | Covered by Suite |
|---|---|---|
| `init()` / `onTick()` / `onExit()` / `onAction()` | Lifecycle hook (runtime) | `lifecyclehooks` |
| `HPR.getLoadedExtensions` | System | `otherextensions` |
| `HPR.unloadExtension` | System | `otherextensions` |
| `HPR.writeCsv` (single-value) | File I/O | `csvio` |
| `HPR.writeCsv` (multi-value `key,v1,v2,...`) | File I/O | `csvio` |
| `HPR.readCsv` (key lookup — scalar) | File I/O | `csvio` |
| `HPR.readCsv` (key lookup — array table) | File I/O | `csvio` |
| `HPR.readCsv` (full-file form) | File I/O | `csvio` |
| `HPR.readCsv` (key not found → `""`) | File I/O | `csvio` |
| `HPR.deleteCsv` | File I/O | `csvio` |
| `HPR.getExtensionDir` | Paths | `csvio` |
| `HPR.getExtensionPath` | Paths | `csvio` |
| `HPR.getCurrentWindow` | Window | `windowbackends` |
| `HPR.getCurrentTitle` | Window | `windowbackends` |
| `HPR.registerBackend` | Window | `windowbackends` |
| `HPR.stopTracking` | Window | `windowbackends` |
| `HPR.startTracking` | Window | `windowbackends` |
| `HPR.startServer` | Networking | `netio` |
| `HPR.httpGet` | Networking | `netio` |
| `HPR.httpPost` | Networking | `netio` |
| `HPR.httpPut` | Networking | `netio` |
| `HPR.httpDelete` | Networking | `netio` |
| `HPR.parseISO8601` | Time | `netio` |
| `HPR.parseJSON` | Serialisation | `netio` |
| `HPR.toJSON` | Serialisation | `netio` |
| `HPR.sleep` | Time | `time` |
| `HPR.convertToDate_DDMMYY` | Time | `time` |
| `HPR.convertToDate_MMYY` | Time | `time` |
| `HPR.convertToTime_HHMMSS_12` | Time | `time` |
| `HPR.formatTime_HHMMSS` | Time | `time` |
| `HPR.parseDate_DDMMYY` | Time | `time` |
| `HPR.parseDate_MMYY` | Time | `time` |
| `HPR.extractMMYY_from_DDMMYY` | Time | `time` |
| `HPR.showUiPopup` | UI | `ui` |
| `HPR.log` | Logger | `logger` |
| `HPR.getLiveTimeLogPerApp` | Live Data | `livedata` |
| `HPR.getLiveTimeLogPerTab` | Live Data | `livedata` |
| `HPR.getLiveTimeLogPerProject` | Live Data | `livedata` |
| `HPR.runSystemCommand` | Process | `process` |
| `HPR.getPid` | Process | `process` |
| `HPR.killPid` | Process | `process` |

> **Note:** `getExtensionDir` and `getExtensionPath` share the same implementation; covering one implicitly covers the other.

---

## ❌ Uncovered APIs

| API | Category | Description |
|---|---|---|
| `HPR.getExtensionAbsoluteDir` | Paths | Absolute filesystem directory path of the extension |
| `HPR.getTime_MS` | Time | Current wall-clock time in milliseconds (Unix epoch) |
| `HPR.dbExecute` | Database | Execute a write SQL statement on today's tracking DB |
| `HPR.dbQuery` | Database | Run a SELECT on today's tracking DB → rows as table |
| `HPR.dbQueryHistorical` | Database | Run a SELECT on the historical DB (waits for async load) |
| `HPR.dbQueryPath` | Database | Run a SELECT on an arbitrary DB file by path |
| `HPR.dbQueryNumber` | Database | Query across the last N days of DBs |
| `HPR.dbQueryRange` | Database | Query across DBs between two `DD/MM/YY` dates |
| `HPR.getLoadedHistDbPath` | Database | Path to the currently loaded historical DB file |
| `HPR.getDbPathForDate` | Database | Resolve the DB file path for a given `DD/MM/YY` date |
| `HPR.getAlias` | Aliases | App alias lookup by raw name |
| `HPR.getAlias_Tab` | Aliases | Tab alias lookup by raw name |
| `HPR.getAlias_Project` | Aliases | Project alias lookup by raw name |
| `HPR.getReverseAlias` | Aliases | Reverse lookup: alias name → raw app name |
| `HPR.getReverseAlias_Tab` | Aliases | Reverse lookup: alias name → raw tab name |
| `HPR.getReverseAlias_Project` | Aliases | Reverse lookup: alias name → raw project name |
| `HPR.getSystemConfig` | Config | Read a value from HPR's `config.csv` by key |
| `HPR.setUiProperty` | UI | Set a Slint UI property by name from Lua |
| `HPR.setUiImage` | UI | Push a raw RGBA pixel buffer to a Slint image property |
| `HPR.registerUiCallback` | UI | Register a Lua function as a Slint UI callback |
| `HPR.showUi` | UI | Show the HPR window |
| `HPR.hideUi` | UI | Hide the HPR window |
| `HPR.quitUi` | UI | Quit the application |
| `HPR.showNotification` | UI | Show an OS desktop notification |
| `HPR.connect` | Events | Subscribe to a system or custom event by name |
| `HPR.disconnect` | Events | Unsubscribe from an event by name and connection ID |
| `HPR.emit` | Events | Emit a system or custom event with optional payload |
| `HPR.generateInsights` | Analytics | Trigger the pattern analyser to generate insights |
| `HPR.getMostUsed` | Analytics | Most-used app string from pattern analyser |
| `HPR.getTotalTrackedTime` | Analytics | Total tracked time string from pattern analyser |
| `HPR.getSwitchCount` | Analytics | App-switch count string from pattern analyser |
| `HPR.getMostSwitchedFrom` | Analytics | App switched away from most often |
| `HPR.getMostSwitchedTo` | Analytics | App switched to most often |
| `HPR.getMostFocusedSession` | Analytics | Longest focused session string |
| `HPR.getMostProductiveHour` | Analytics | Most productive hour-of-day string |
| `HPR.getEscapePattern` | Analytics | Pattern of context-switching away from an app |
| `HPR.getReturnRate` | Analytics | Rate at which users return to an app after leaving |
| `HPR.getAvgFocusSession` | Analytics | Average focused session duration string |
| `HPR.getMostDistractedDay` | Analytics | Day of the week with the most app switches |
| `HPR.getProductiveDaysThisWeek` | Analytics | Count of productive days in the current week |
| `HPR.getScreenTimeVsAverage` | Analytics | Current screen time compared to daily average |
| `HPR.getFocusDipHour` | Analytics | Hour of day where focus is typically lowest |
| `HPR.getDeepWorkRelativeNoon_Random` | Analytics | Deep work completed before noon (random selection) |
| `HPR.getWeekendVsWeekday` | Analytics | Weekend vs weekday usage comparison string |
| `HPR.setLimit` | Limits | Set minute time limit for app, tab, or project |
| `HPR.getLimit` | Limits | Get configured minute time limit for app, tab, or project |
| `HPR.setGoal` | Goals | Set minute daily target goal for app, tab, or project |
| `HPR.getGoal` | Goals | Get configured minute daily target goal for app, tab, or project |
| `HPR.getOsName` | System | Returns `"Windows"`, `"Linux"`, or `"Apple"` |
| `HPR.getEnvironmentName` | System | Returns `$XDG_CURRENT_DESKTOP` on Linux; empty on Windows |
| `HPR.reloadExtension` | System | Reload another extension by author + name |
| `HPR.reloadMyself` | System | Reload the calling extension itself |
| `HPR.refreshExtensions` | System | Hot-load any new `.lua` files dropped into the extensions dir |
| `HPR.applyTheme` | System | Apply a UI theme by name (interpreter mode only) |
| `HPR.getThemeNames` | System | Returns array of all available UI theme names |
| `HPR.getCurrentThemeName` | System | Returns the name of the currently active UI theme |
| `HPR.getHPRVersion` | System | Returns the current HPR version string |
| `HPR.amICompatible` | System | Returns whether extension is marked compatible with current HPR version |
| `HPR.setVersionSupport` | System | Declare which HPR versions the extension supports (runtime call) |
| `HPR.setLegacyAPISuffix` | System | Enable/disable legacy `_E` API suffix at runtime |
| `HPR.setUseHPRTablePrefix` | System | Enable/disable requiring `HPR.` prefix for all functions |
| `HPR.crash` | Debug | Intentionally crash HPR with an optional message |
