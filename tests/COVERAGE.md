# HPR Lua Extension API — Test Coverage

> Auto-derived from [`extensionManager.cpp`](../src/extension/extensionManager.cpp).
> Update this document whenever a new suite is added or a new API is registered.

## Summary

| Metric | Count |
|---|---|
| **Total API surface** | 71 |
| **Covered** | 5 |
| **Uncovered** | 66 |
| **Coverage** | 7.04% |

---

## ✅ Covered APIs

| API | Category | Covered by Suite |
|---|---|---|
| `init()` | Lifecycle hook | `lifecyclehooks` |
| `onTick(delta)` | Lifecycle hook | `lifecyclehooks` |
| `onExit()` | Lifecycle hook | `lifecyclehooks` |
| `HPR.getLoadedExtensions_E` | System | `otherextensions` |
| `HPR.unloadExtension_E` | System | `otherextensions` |

---

## ❌ Uncovered APIs

| API | Category | Description |
|---|---|---|
| `HPR.writeCsv_E` | File I/O | Write key-value pair to a CSV file in the extension dir |
| `HPR.getExtensionDir_E` | Paths | Get relative path of current extension directory |
| `HPR.log_E` | Logging | Log a message to HPR log output |
| `HPR.getExtensionPath_E` | Paths | Alias of `getExtensionDir_E` — returns relative extension subdir path |
| `HPR.startServer_E` | Networking | Start an embedded HTTP server on a given port with a request handler |
| `HPR.httpGet_E` | Networking | HTTP GET request (host, path, secure?, headers?) → (body, status) |
| `HPR.httpPost_E` | Networking | HTTP POST request with body → (body, status) |
| `HPR.httpPut_E` | Networking | HTTP PUT request with body → (body, status) |
| `HPR.httpDelete_E` | Networking | HTTP DELETE request → (body, status) |
| `HPR.parseJSON_E` | Serialisation | Parse a JSON string; optional field-path to extract a nested value |
| `HPR.toJSON_E` | Serialisation | Serialise a Lua table to a JSON string |
| `HPR.getTime_MS_E` | Time | Current wall-clock time in milliseconds (Unix epoch) |
| `HPR.sleep_E` | Time | Sleep for N ms, respecting extension shutdown signal |
| `HPR.parseISO8601_E` | Time | Parse ISO-8601 datetime string → Unix ms timestamp |
| `HPR.convertToDate_DDMMYY_E` | Time | Convert Unix ms → `DD/MM/YY` string |
| `HPR.convertToDate_MMYY_E` | Time | Convert Unix ms → `MM/YY` string |
| `HPR.convertToTime_HHMMSS_12_E` | Time | Convert Unix ms → 12-hour `HH:MM:SS AM/PM` string |
| `HPR.formatTime_HHMMSS_E` | Time | Format a duration in ms as `HH:MM:SS` |
| `HPR.parseDate_DDMMYY_E` | Time | Parse `DD/MM/YY` string → Unix ms |
| `HPR.parseDate_MMYY_E` | Time | Parse `MM/YY` string → Unix ms |
| `HPR.extractMMYY_from_DDMMYY_E` | Time | Extract `MM/YY` portion from a `DD/MM/YY` string |
| `HPR.getCurrentWindow_E` | Window | Name of the currently focused application window |
| `HPR.getCurrentTitle_E` | Window | Title of the currently focused application window |
| `HPR.stopTracking_E` | Window | Pause window focus tracking |
| `HPR.startTracking_E` | Window | Resume window focus tracking |
| `HPR.registerBackend_E` | Window | Register a custom window-tracking backend (Lua-implemented) |
| `HPR.getLiveTimeLogPerApp_E` | Live Data | Map of app name → ms tracked today (live, current session) |
| `HPR.getLiveTimeLogPerTab_E` | Live Data | Map of browser tab title → ms tracked today |
| `HPR.getLiveTimeLogPerProject_E` | Live Data | Map of project name → ms tracked today |
| `HPR.getPid_E` | Process | PID string for a given raw app name |
| `HPR.killPid_E` | Process | Force-kill a process by PID string |
| `HPR.runSystemCommand_E` | Process | Run a shell command and return its stdout |
| `HPR.dbExecute_E` | Database | Execute a write SQL statement on today's tracking DB |
| `HPR.dbQuery_E` | Database | Run a SELECT on today's tracking DB → rows as table |
| `HPR.dbQueryHistorical_E` | Database | Run a SELECT on the historical DB (waits for async load) |
| `HPR.dbQueryPath_E` | Database | Run a SELECT on an arbitrary DB file by path |
| `HPR.dbQueryNumber_E` | Database | Query across the last N days of DBs |
| `HPR.dbQueryRange_E` | Database | Query across DBs between two `DD/MM/YY` dates |
| `HPR.getLoadedHistDbPath_E` | Database | Path to the currently loaded historical DB file |
| `HPR.getDbPathForDate_E` | Database | Resolve the DB file path for a given `DD/MM/YY` date |
| `HPR.getAlias_E` | Aliases | App alias lookup by raw name |
| `HPR.getAlias_Tab_E` | Aliases | Tab alias lookup by raw name |
| `HPR.getAlias_Project_E` | Aliases | Project alias lookup by raw name |
| `HPR.getReverseAlias_E` | Aliases | Reverse lookup: alias name → raw app name |
| `HPR.getReverseAlias_Tab_E` | Aliases | Reverse lookup: alias name → raw tab name |
| `HPR.getReverseAlias_Project_E` | Aliases | Reverse lookup: alias name → raw project name |
| `HPR.getSystemConfig_E` | Config | Read a value from HPR's `config.csv` by key |
| `HPR.readCsv_E` | File I/O | Read all or a single key from a CSV file in the extension dir |
| `HPR.deleteCsv_E` | File I/O | Delete a CSV file in the extension dir |
| `HPR.setUiProperty_E` | UI | Set a Slint UI property by name from Lua |
| `HPR.setUiImage_E` | UI | Push a raw RGBA pixel buffer to a Slint image property |
| `HPR.registerUiCallback_E` | UI | Register a Lua function as a Slint UI callback |
| `HPR.showUi_E` | UI | Show the HPR window |
| `HPR.hideUi_E` | UI | Hide the HPR window |
| `HPR.quitUi_E` | UI | Quit the application |
| `HPR.showNotification_E` | UI | Show an OS desktop notification |
| `HPR.connect_E` | Events | Subscribe to a system or custom event by name |
| `HPR.disconnect_E` | Events | Unsubscribe from an event by name and connection ID |
| `HPR.emit_E` | Events | Emit a system or custom event with optional payload |
| `HPR.generateInsights_E` | Analytics | Trigger the pattern analyser to generate insights |
| `HPR.getMostUsed_E` | Analytics | Most-used app string from pattern analyser |
| `HPR.getTotalTrackedTime_E` | Analytics | Total tracked time string from pattern analyser |
| `HPR.getSwitchCount_E` | Analytics | App-switch count string from pattern analyser |
| `HPR.getMostSwitchedFrom_E` | Analytics | App switched away from most often |
| `HPR.getMostSwitchedTo_E` | Analytics | App switched to most often |
| `HPR.getMostFocusedSession_E` | Analytics | Longest focused session string |
| `HPR.getMostProductiveHour_E` | Analytics | Most productive hour-of-day string |
| `HPR.getOsName_E` | System | Returns `"Windows"`, `"Linux"`, or `"Apple"` |
| `HPR.getEnvironmentName_E` | System | Returns `$XDG_CURRENT_DESKTOP` on Linux; empty on Windows |
| `HPR.reloadExtension_E` | System | Reload another extension by author + name |
| `HPR.refreshExtensions_E` | System | Hot-load any new `.lua` files dropped into the extensions dir |
| `HPR.applyTheme_E` | System | Apply a UI theme by name (interpreter mode only) |
| `HPR.crash_E` | Debug | Intentionally crash HPR with an optional message |
| `HPR.overrides` | Override table | Register override functions (`HPR.overrides.stopTracking`, etc.) |
