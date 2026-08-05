# HPR Lua Extension API — Test Coverage

> Auto-derived from [`extensionManager.cpp`](../src/extension/extensionManager.cpp).
> Update this document whenever a new suite is added or a new API is registered.

## Summary

| Metric | Count |
|---|---|
| **Total API surface** | 79 |
| **Covered** | 28 |
| **Uncovered** | 51 |
| **Coverage** | 35.44% |

---

## ✅ Covered APIs

| API | Category | Covered by Suite |
|---|---|---|
| `init()` / `onTick()` / `onExit()` | Lifecycle hook | `lifecyclehooks` |
| `HPR.getLoadedExtensions` | System | `otherextensions` |
| `HPR.unloadExtension` | System | `otherextensions` |
| `HPR.writeCsv` | File I/O | `csvio` |
| `HPR.readCsv` | File I/O | `csvio` |
| `HPR.deleteCsv` | File I/O | `csvio` |
| `HPR.getExtensionDir` | Paths | `csvio` |
| `HPR.getExtensionPath` | Paths | `csvio` |
| `HPR.getCurrentWindow` | Window | `windowbackends` |
| `HPR.getCurrentTitle` | Window | `windowbackends` |
| `HPR.registerBackend` | Window | `windowbackends` |
| `HPR.startServer` | Networking | `netio` |
| `HPR.httpGet` | Networking | `netio` |
| `HPR.httpPost` | Networking | `netio` |
| `HPR.httpPut` | Networking | `netio` |
| `HPR.httpDelete` | Networking | `netio` |
| `HPR.parseISO8601` | Time | `netio` |
| `HPR.parseJSON` | Serialisation | `netio` |
| `HPR.toJSON` | Serialisation | `netio` |
| `HPR.convertToDate_DDMMYY` | Time | `time` |
| `HPR.convertToDate_MMYY` | Time | `time` |
| `HPR.convertToTime_HHMMSS_12` | Time | `time` |
| `HPR.formatTime_HHMMSS` | Time | `time` |
| `HPR.parseDate_DDMMYY` | Time | `time` |
| `HPR.parseDate_MMYY` | Time | `time` |
| `HPR.extractMMYY_from_DDMMYY` | Time | `time` |

> **Note:** `getExtensionDir` and `getExtensionPath` share the same implementation, covering one implicitly covers the other.

---

## ❌ Uncovered APIs

| API | Category | Description |
|---|---|---|
| `HPR.getTime_MS` | Time | Current wall-clock time in milliseconds (Unix epoch) |
| `HPR.sleep` | Time | Sleep for N ms, respecting extension shutdown signal |
| `HPR.stopTracking` | Window | Pause window focus tracking |
| `HPR.startTracking` | Window | Resume window focus tracking |
| `HPR.getLiveTimeLogPerApp` | Live Data | Map of app name → ms tracked today (live, current session) |
| `HPR.getLiveTimeLogPerTab` | Live Data | Map of browser tab title → ms tracked today |
| `HPR.getLiveTimeLogPerProject` | Live Data | Map of project name → ms tracked today |
| `HPR.getPid` | Process | PID string for a given raw app name |
| `HPR.killPid` | Process | Force-kill a process by PID string |
| `HPR.runSystemCommand` | Process | Run a shell command and return its stdout |
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
| `HPR.showUiPopup` | UI | Show a custom popup with left/right buttons executing a callback |
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
| `HPR.getOsName` | System | Returns `"Windows"`, `"Linux"`, or `"Apple"` |
| `HPR.getEnvironmentName` | System | Returns `$XDG_CURRENT_DESKTOP` on Linux; empty on Windows |
| `HPR.reloadExtension` | System | Reload another extension by author + name |
| `HPR.refreshExtensions` | System | Hot-load any new `.lua` files dropped into the extensions dir |
| `HPR.applyTheme` | System | Apply a UI theme by name (interpreter mode only) |
| `HPR.crash` | Debug | Intentionally crash HPR with an optional message |
| `HPR.overrides` | Override table | Register override functions (`HPR.overrides.stopTracking`, etc.) |
