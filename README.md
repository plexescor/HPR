# HPR — Human Pattern Recorder

A lightweight, local-first activity tracker for Windows and Linux. HPR runs silently in the background, detects which application you are currently using, and builds a precise log of how your time is actually spent. No accounts. No cloud. No telemetry. Everything stays on your machine, organized into per-day SQLite databases that you fully own and control.

---

## What HPR Does

HPR monitors the active window on your system and tracks:

- Time spent in each application, accumulated with millisecond precision
- Every application switch, recorded with an exact timestamp
- Cumulative per-app usage for the current session and across past sessions

On launch, HPR restores your historical data from today's database file and continues accumulating from where it left off. When you close HPR, all data is safely flushed to disk. Nothing is lost between sessions.

The data is stored as standard SQLite files. You can open them with any SQLite browser, query them with any SQL tool, or delete specific days or months by simply removing the corresponding folder.

---

## Current State (v0.1)

HPR is functional and actively developed. The following is working today:

- Real-time active window detection on Hyprland (Wayland), GNOME (Wayland), and Windows
- Millisecond-accurate time tracking per application using steady clock intervals
- Application switch history with precise system-clock timestamps for each transition
- Per-day SQLite persistence organized as `~/HPR_DB/MM-YY/DD-MM-YY.db`
- Session restoration on launch — today's historical data is loaded back into memory before tracking begins
- Human-readable duration formatting in the UI (displayed as `2h 14m 30s` rather than raw milliseconds)
- Live UI updates showing current window, time per app, and switch history
- Automatic GNOME Shell extension setup for window detection on GNOME Wayland

What is not yet complete:

- Visual UI design — the current interface is functional but unstyled
- Insights and analytics — planned for upcoming versions
- Data export (CSV, JSON)
- Historical session browsing
- macOS support

---

## How It Works

HPR runs three background threads alongside the main UI thread.

The window polling thread queries the active window every 50 milliseconds using platform-native methods. On Hyprland it calls `hyprctl activewindow` and parses the JSON output. On GNOME it communicates with the `window-calls-extended` shell extension over D-Bus. On Windows it uses the Win32 `GetForegroundWindow` API. The raw window name is normalized through a validation pass that strips shell output artifacts, filters system processes, and maps known application names to human-readable labels.

The tracking and UI bridge thread reads from the shared application state every 100 milliseconds, converts the data to types compatible with the Slint UI framework, and dispatches updates to the main thread through a thread-safe event loop callback. This thread never touches the UI directly — it posts all updates via `slint::invoke_from_event_loop` using a weak handle to prevent crashes if the window is closing.

The database writer thread copies the current state from shared memory every 10 seconds and flushes it to SQLite. App usage records use `INSERT OR REPLACE` semantics so each application always has exactly one up-to-date row. Switch history records use `INSERT OR IGNORE` with a unique timestamp constraint so each switch event is written exactly once regardless of how many flush cycles occur.

All three threads share a single `AppState` struct protected by a single `std::mutex`. Every read and write goes through a `std::lock_guard` on that mutex. Thread lifecycle is managed via `std::atomic<bool>` stop flags and `std::thread::join` in each class destructor.

---

## Data Organization

HPR organizes data as follows:

```
~/HPR_DB/
    05-26/
        01-05-26.db
        02-05-26.db
        03-05-26.db
    04-26/
        30-04-26.db
```

One database file per day. One folder per month. To delete all data from April, remove the `04-26` folder. To inspect any day's data, open the corresponding `.db` file with any SQLite tool. Each database contains two tables:

`app_usage` — one row per application with columns `name` (unique) and `duration` (milliseconds).

`switch_history` — one row per switch event with columns `fromWindow`, `toWindow`, and `timeStamp` (Unix milliseconds, unique).

Typical database size is 30 to 100 kilobytes per day. A full year of data for a heavy user is well under 50 megabytes.

---

## Platform Support

**Hyprland (Linux, Wayland)**

Detection is done via `hyprctl activewindow -j | jq -r '.class'`. This is the cleanest integration — one command, reliable output, no extensions required. HPR was primarily developed and tested on Hyprland.

**GNOME (Linux, Wayland)**

GNOME on Wayland does not expose focused window information without a shell extension. HPR automatically handles this: on first launch it checks whether the `window-calls-extended` extension is responding. If it is not, HPR clones the extension from GitHub into the correct local extensions directory and enables it. Because GNOME on Wayland cannot reload shell extensions without a session restart, HPR will prompt you to log out and back in. This is a GNOME limitation, not an HPR limitation. After the one-time setup, subsequent launches work without any manual steps.

**Windows (10/11)**

Detection uses `GetForegroundWindow` combined with `GetWindowTextA` from the Win32 API. The binary is built as a `WIN32` subsystem executable with no visible console window.

**macOS**

Not currently supported. May be added in a future version.

---

## Window Name Normalization

Raw window identifiers from platform APIs are often inconsistent — they may contain trailing newlines, shell-specific formatting characters, or vary in casing between versions. HPR normalizes every window name before logging it:

1. Trailing newline and carriage return characters are stripped
2. The name is lowercased for comparison purposes
3. System processes that should not be tracked (Windows shell host processes, empty strings) are mapped to `Unknown`
4. Common applications are mapped to consistent display names regardless of how the platform reports them (`chrome` becomes `Chrome`, `code` becomes `Visual Studio Code`, and so on)
5. The original casing is preserved for applications that do not match any known pattern

---

## Building From Source

Requirements:

- CMake 3.21 or later
- A C++23-capable compiler (GCC 13+, Clang 16+, or MSVC 2022+)
- Slint (installed system-wide, or CMake will fetch it automatically from the Slint GitHub repository)
- On Linux: `jq` for Hyprland support, `gdbus` for GNOME support

```bash
git clone https://github.com/plexescor/HPR
cd HPR
mkdir build
cd build
cmake ..
cmake --build . --parallel
```

On first build, if Slint is not found on the system, CMake will download and compile it automatically. This takes several minutes. Subsequent builds are fast.

The SQLite library is included in the repository as the official amalgamation source file and compiled directly into the binary. No system SQLite installation is required.

---

## Comparison

| Feature | HPR | ActivityWatch | RescueTime | Toggl |
|---|---|---|---|---|
| Binary size | ~4 MB | 200 MB+ | Cloud-based | Cloud-based |
| RAM usage | under 30 MB | 200 MB+ | N/A | N/A |
| Data storage | Local SQLite files | Local (web server) | Cloud | Cloud |
| Requires account | No | No | Yes | Yes |
| Auto-tracking | Yes | Yes | Yes | No |
| Wayland support | Yes | Partial | N/A | N/A |
| Open source | Yes | Yes | No | No |
| Free | Yes (premium planned) | Yes | Limited | Limited |
| Requires running server | No | Yes | No | No |
| Startup time | Instant | Several seconds | N/A | N/A |

---

## Planned Features

**Near term**

- Visual UI design with proper layout, typography, and dark theme
- Basic analytics: most-used application, longest focus session, total tracked time, switch frequency
- Data export to CSV and JSON
- Historical session viewer

**Premium tier (planned)**

- LLM-powered pattern analysis and personalized insights
- Focus mode with application blocking
- Advanced productivity reports
- Cross-device data aggregation (optional, local network only)

---

## Technical Details For Contributors

The codebase is written in C++23 and makes use of structured bindings, `std::string::contains`, `std::filesystem`, `std::optional`, and `std::atomic` throughout. The threading model uses three worker threads plus the main Slint event loop thread. Shared state is managed through a single globally-accessible `AppState` struct behind a `std::mutex`, using `std::lock_guard` consistently for all access.

The UI is built with Slint 1.x. Slint's declarative `.slint` language defines the component structure and data bindings. Data is passed from C++ to the UI via `slint::VectorModel` wrapped in `std::shared_ptr`, pushed through `slint::invoke_from_event_loop` using a `slint::ComponentWeakHandle` to ensure crash safety during window teardown.

The database layer uses the `sqlite_modern_cpp` header-only wrapper over the SQLite3 amalgamation. The connection is held open for the lifetime of the `DatabaseManager` object rather than opened and closed per write.

All header files use `#pragma once`. Platform-specific code is isolated behind `#ifdef _WIN32` and `#ifdef __linux__` guards. The CMake build handles both platforms with separate `add_executable` targets.

---

## FAQ

**Does HPR take screenshots?**
No. HPR only reads the name of the focused application window. No screen content, keystrokes, or mouse activity is ever captured.

**Does any data leave my machine?**
No. HPR contains no networking code. The only outbound operation is the one-time `git clone` of the GNOME extension on GNOME systems, which is a shell command executed by the OS, not HPR itself. After setup, there is zero external communication.

**Can I delete my data?**
Yes. Your data is in `~/HPR_DB`. Delete any file or folder and it is gone permanently. HPR does not maintain any other data store.

**What happens if HPR crashes?**
Data from the current 10-second flush interval may be lost. All previously flushed data is safe in the SQLite file. The database connection uses standard SQLite durability guarantees.

**Why not use a single database file for all history?**
Per-day files make data management intuitive. Deleting a month is removing a folder. Inspecting a specific day means opening one small file. The files stay small and fast to query throughout the application's lifetime.

---

## License

MIT License. See LICENSE for details.

---

**Status:** Active development, v0.1
**Platforms:** Hyprland (Linux/Wayland), GNOME (Linux/Wayland), Windows 10/11
**Language:** C++23
**Dependencies:** Slint (UI), SQLite3 (bundled), sqlite_modern_cpp (bundled)