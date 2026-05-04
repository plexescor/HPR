# HPR - Human Pattern Recorder

A lightweight, offline activity tracker for Windows and Linux. HPR runs silently in the background and tells you exactly where your time goes, down to the millisecond, without ever talking to a server, requiring an account, or eating your RAM.

> Currently in active development. Core tracking and persistence are fully functional. UI is bare-bones but the data layer is solid.

---
---

# FOR USERS

---

## What Is HPR

HPR is an application tracker. It watches which window is active on your screen and builds a log of how much time you spend in each application, every day. When you switch from Chrome to VSCode, it records that. When you come back, it picks up where it left off. When you close and reopen HPR the next day, your history from today is already loaded.

Everything it records lives in a folder on your machine called `HPR_DB` inside your home directory. Nothing goes anywhere else. There is no server, no account, no API key, no analytics endpoint. The only internet activity that ever happens is a one-time Git clone on GNOME systems to set up a required shell extension, and that is a shell command the OS runs, not HPR itself.

## What HPR Is Not

HPR does not take screenshots. It does not log keystrokes. It does not record mouse movement. It does not read the contents of your windows. It reads exactly one thing: the name of the currently active application. That is the full scope of what it captures.

HPR is also not a subscription service, not a SaaS dashboard, and not an Electron app. It is a compiled C++ binary. It starts in milliseconds and uses under 30MB of RAM while running.

## What It Actually Shows You

At the moment HPR shows you three things in real time:

The name of the application you are currently in. The total time you have spent in each application today, shown in a human readable format like `2h 14m 30s`. The history of every application switch you made, showing which app you switched from, which app you switched to, and at what time the switch happened.

This is the foundation. Analytics, insights, focus mode, and more are planned and described in the roadmap section below.

## Where Your Data Goes

HPR stores your data in SQLite database files organized like this:

```
~/HPR_DB/
    05-26/
        01-05-26.db
        02-05-26.db
        03-05-26.db
    04-26/
        30-04-26.db
```

One file per day. One folder per month. To delete everything from last month, delete the folder. To see exactly what was recorded on any specific day, open that day's `.db` file with any SQLite viewer. The files are completely standard SQLite, compatible with every SQLite tool ever made. A typical day of usage produces a file somewhere between 30 and 100 kilobytes. A full year of data is well under 50 megabytes.

## Platform Support

HPR works on Hyprland (Linux, Wayland), GNOME (Linux, Wayland), KDE Plasma (Linux, Wayland/X11), and Windows 10 and 11.

On Hyprland, setup is zero effort. HPR queries the compositor directly using `hyprctl` and everything works on first launch.

On GNOME with Wayland, HPR requires a shell extension called `window-calls-extended` to expose the focused window information. On first launch, HPR checks whether the extension is already working. If it is not, HPR will prompt you to run the `installWindowCallsExtension.sh` script that ships next to the binary. That script clones and enables the extension for you. Because GNOME on Wayland cannot reload shell extensions without a session restart, you will need to log out and back in once after running it. After that one-time setup, subsequent launches work without any intervention.

On KDE Plasma, HPR uses KWin's scripting API via `qdbus6` to query the active window. No additional setup is required.

On Windows, HPR uses the standard Win32 API to query the active window. The binary is built without a console window so it runs cleanly in the background.

## Comparison With Other Trackers

| Feature | HPR | ActivityWatch | RescueTime | Toggl |
|---|---|---|---|---|
| Binary size | ~4 MB | 200 MB+ | Cloud app | Cloud app |
| RAM usage | Under 30 MB | 200 MB+ | N/A | N/A |
| Requires account | No | No | Yes | Yes |
| Data leaves your machine | Never | Never | Yes | Yes |
| Auto-tracking | Yes | Yes | Yes | No |
| Wayland support | Yes | Partial | N/A | N/A |
| Requires running web server | No | Yes | No | No |
| Open source | Yes | Yes | No | No |
| Startup time | Instant | Several seconds | N/A | N/A |
| Free | Yes (premium planned) | Yes | Limited | Limited |

The closest honest comparison is ActivityWatch. ActivityWatch is a mature project with a full web dashboard, browser extensions, plugin ecosystem, and multiple years of development. HPR is early-stage and has none of those things yet. What HPR has that ActivityWatch does not is a significantly smaller footprint, native Wayland support from day one, no embedded web server, and no Python runtime. If you want a mature tool today, use ActivityWatch. If you want something that will eventually be faster and leaner, HPR is being built for that.

## Roadmap

The following is what is actually planned in roughly the order it will be built.

Visual UI redesign is the immediate next step. The current interface shows raw data correctly but has no styling, no layout design, and no visual hierarchy. This is known and being addressed.

Human-readable insights derived entirely from code, no LLM involved. Things like most-used application today, longest uninterrupted focus session, total tracked time, and which application you switch away from most frequently. These are simple calculations on data HPR already has.

Historical session browser so you can look at past days without opening SQLite files manually.

Data export to CSV and JSON.

A premium tier is planned for the future. The free tier will always include full local tracking, full data ownership, and the code-derived basic insights. The premium tier is intended to include LLM-powered pattern analysis that can read your usage data and give you personalized observations about your working patterns, focus mode with application blocking, and advanced reporting. This is not imminent.

---
---

# FOR DEVELOPERS AND POWER USERS

---

## Architecture Overview

HPR is a multi-threaded C++23 application. The threading model is the most important thing to understand about the codebase. There are four threads running at runtime.

The main thread is the Slint event loop. It handles window events, input, and UI repaints. It does nothing else. It blocks on `(*ui)->run()` for the entire lifetime of the application.

The window polling thread runs inside `CurrentWindowManager` and calls the platform-specific active window getter every 50 milliseconds using `std::this_thread::sleep_for`. It writes the current window name and elapsed time into the shared `AppState` struct behind a mutex on every tick.

The UI bridge thread runs inside `HPR` and reads from `AppState` every 100 milliseconds, converts the data into Slint-compatible types, and posts updates to the main thread using `slint::invoke_from_event_loop`. It never touches the UI directly. It captures everything by value into the lambda so the snapshot it pushes is always consistent regardless of what the polling thread does between capture and dispatch.

The database writer thread runs inside `DatabaseManager` and wakes every 10 seconds to flush a snapshot of `AppState` to SQLite. It uses a chunked sleep of 100 intervals of 100 milliseconds each so that it can respond to the stop flag quickly when the application is closing, rather than blocking shutdown for up to 10 seconds.

## Shared State

All shared mutable state lives in a single struct inside the `AppState` namespace:

```cpp
namespace AppState {
    struct AppState {
        std::string currentWindow;
        std::string previousWindow;
        std::map<std::string, long> timeLog_PerApp;
        std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;
    };
    extern AppState state;
    extern std::mutex stateMutex;
}
```

`state` and `stateMutex` are declared `extern` in the header and defined once in `appState.cpp`. Every thread accesses the state through `std::lock_guard<std::mutex>`. There are no fine-grained per-field locks. The mutex covers the entire struct. For the current scale of this application this is the correct tradeoff.

`switchHistory` maps a pair of application names to a vector of Unix millisecond timestamps. Each element in the vector represents one instance of that specific transition occurring. So if you switched from Chrome to VSCode three times today, the vector for `{Chrome, VSCode}` contains three timestamps.

## Database Layer

HPR uses `sqlite_modern_cpp`, a header-only C++ wrapper over the SQLite3 amalgamation. The SQLite3 amalgamation is compiled directly into the binary as a C source file. Neither dependency requires system installation.

The `DatabaseManager` holds the database connection as `std::optional<sqlite::database>`. The optional is emplaced in the constructor using `db.emplace(filePath + fileName)` after the file path is resolved. The connection stays alive for the entire lifetime of the object and is destroyed when the destructor runs. This is not the same as opening and closing the file on every write, which would be expensive and incorrect.

The two tables have different write strategies because they have different semantics.

`app_usage` has a `UNIQUE` constraint on the `name` column. It uses `INSERT OR REPLACE`. When a new flush happens for Chrome, the existing Chrome row is deleted and a fresh one with the updated duration is inserted. There is always exactly one row per application.

`switch_history` has a `UNIQUE` constraint on the `timeStamp` column. It uses `INSERT OR IGNORE`. Because the write loop copies the entire `switchHistory` map every 10 seconds and tries to insert all timestamps again, the unique constraint ensures each switch event is persisted exactly once. Subsequent insert attempts for the same timestamp are silently skipped.

On startup, `loadStateFromDB()` is called before any background threads are started. It reads both tables and populates `AppState::state`. Because the threads have not started yet, no mutex is needed during the initial load and there are no race conditions.

## Timing Model

HPR uses two different clocks deliberately.

`std::chrono::steady_clock` is used to measure elapsed time between polling ticks. Steady clock is guaranteed to only move forward at a constant rate. It is never adjusted by NTP, never jumps for DST changes, and never moves backward. This makes it correct for accumulating duration measurements.

`std::chrono::system_clock` is used to record the wall-clock time of switch events. System clock gives you the actual human-readable time, which is what you want when recording that a switch happened at 5:38pm. These timestamps are stored as Unix milliseconds in the database and converted to human-readable strings only at display time.

Using system clock for elapsed duration measurements would be incorrect because NTP adjustments or DST changes could cause time to jump mid-session, making duration accumulation wrong.

## Window Name Normalization

Every raw window name returned by the platform getter passes through `validateAndUpdateWindow_Cross` before being written to state. This function:

Strips trailing newline and carriage return characters from shell command output. Produces a lowercase copy of the name for comparison purposes while retaining the original-case version for display. Maps known garbage processes to the `Unknown` string so they are not tracked. Maps known application identifiers to consistent human-readable names using C++23 `std::string::contains`. Returns the original-case unmodified name for applications that do not match any known pattern.

The filter and mapping lists are hardcoded and currently cover around 25 common applications across Windows and Linux (Chrome, Firefox, Edge, VSCode, Visual Studio, Discord, Spotify, Steam, OBS, Obsidian, Postman, Slack, Teams, Zoom, VLC, Notion, and a range of terminal emulators). Adding a new application mapping is one `else if` line in `validateAndUpdateWindow.cpp`.

## Class Lifecycle Pattern

Every class with a background thread follows the same pattern. Constructor does setup. `run()` spawns the thread. The destructor sets the atomic stop flag to false and calls `join()`. The thread loop checks the stop flag on every iteration. This is consistent across `CurrentWindowManager`, `HPR`, and `DatabaseManager`.

```cpp
~SomeClass() {
    running = false;
    if (thread.joinable()) thread.join();
}
```

This means every object in `main.cpp` that goes out of scope at program exit will cleanly stop its thread before the destructor returns. Shutdown is orderly and deterministic.

## Building From Source

Requirements:

- CMake 3.21 or later
- GCC 13+, Clang 16+, or MSVC 2022+ with C++23 support
- Slint 1.16.1 (install with the provided script, see below)
- On Linux: `jq` for Hyprland, `gdbus` for GNOME, `qdbus6` for KDE (all standard on their respective distros)

Install Slint and its tooling system-wide first using the provided script:

```bash
git clone https://github.com/plexescor/HPR
cd HPR
sudo ./installDependencies.sh
```

This downloads Slint 1.16.1, slint-lsp, and slint-viewer from GitHub releases, extracts them into `external/`, and installs them to `/usr/local/`. Then build:

```bash
mkdir build && cd build
cmake ..
cmake --build . --parallel
```

SQLite is bundled as the official single-file amalgamation and compiled as part of the build. No system SQLite is required. On Windows use `installDependencies.bat` instead of the shell script.

## Adding A New Platform

All platform-specific window detection is inside `CurrentWindowManager`. The constructor detects the platform by reading `$XDG_CURRENT_DESKTOP` on Linux. The `getCurrentWindow()` method dispatches to the correct platform getter based on the detected platform string.

To add a new platform:

Add a new private method `getCurrentWindow_YourPlatform()` to the class. Implement it in `getCurrentWindow.cpp`. Add an `else if (currentPlatform.contains("YourPlatform"))` branch to the `getCurrentWindow()` dispatcher. Add any setup logic needed to the constructor behind the appropriate preprocessor guard.

The normalization step runs after every platform getter returns, so new platform implementations do not need to handle stripping or name mapping.

## Adding New Tracked Data

To add a new tracked metric:

Add the field to `AppState::AppState` in `appState.hpp`. Add a write to the field in `getCurrentWindow_Loop()` inside the existing mutex lock. Add a read in `HPR::trackingLoop()` to convert it for display. Add a new Slint struct and property in `app-window.slint` if it needs to appear in the UI. Add a table and read/write logic in `DatabaseManager` if it needs to be persisted.

The architecture is additive. Nothing else needs to change.

## Known Issues and Honest Limitations

Raw pointers to `AppState` fields in `HPR::trackingLoop` (`timeLog` and `switchHistory` are raw pointers set before the loop starts) are used only inside the mutex lock where they are safe. They are not used outside the lock. This is safe but unnecessary and slightly misleading. Direct access to `AppState::state` inside the lock would be cleaner.

On GNOME, if the `window-calls-extended` extension is not detected on startup, HPR sets the platform to `GNOME_NO_EXTENSION` and returns a string instructing the user to run `installWindowCallsExtension.sh` (shipped next to the binary and copied to the build directory by CMake). HPR does not automatically run the script itself. The script clones the extension repo and enables it via `gnome-extensions enable`, then prints a prompt to log out and back in.

The KDE backend uses a slow polling approach: it writes a temporary JS file to `/tmp`, loads it as a KWin script via `qdbus6`, waits 100ms, reads the result from the journal, then unloads the script. This runs on every poll tick and is noticeably heavier than the other backends. It works but a cleaner KDE path is a known future improvement.

## Contributing

The codebase is small enough to read entirely in one sitting. Start with `main.cpp` to understand initialization order, then `appState.hpp` to understand the data model, then work through each class. The most important thing to understand before making changes is the threading model: which thread reads what, which thread writes what, and where the mutex must be held.

There is no formal contribution process defined yet. Open an issue or a pull request and we will figure it out.

---

**Status:** Active development, v0.1
**Platforms:** Hyprland (Wayland), GNOME (Wayland), KDE Plasma, Windows 10/11
**Language:** C++23
**UI:** Slint 1.16.1
**Database:** SQLite3 (bundled amalgamation) via sqlite_modern_cpp