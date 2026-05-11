# HPR - Human Pattern Recorder

<p align="center">
  <a href="https://ko-fi.com/plexescor">
    <img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="Support on Ko-fi" />
  </a>
</p>

<p align="center">
  <img src="./assets/logo_1254png.png" alt="HPR Logo" width="350"/>
</p>

> Recursion

A lightweight, offline activity tracker for Windows and Linux. HPR runs silently in the background and tells you exactly where your time goes, down to the millisecond, without ever talking to a server, requiring an account, or eating your RAM.

> Currently in active development. Core tracking and persistence are fully functional. UI is pretty, the data is real.

<p align="center">
  <img src="./assets/HPRHome.png" alt="HPR Home Screen" width="800"/>
</p>

---
---

# FOR USERS

---

## What Is HPR

HPR is an application tracker. It watches which window is active on your screen and builds a log of how much time you spend in each application, every day. When you switch from Chrome to VSCode, it records that. When you come back, it picks up where it left off. When you close and reopen HPR the next day, your history from today is already loaded.

Everything it records lives in a folder on your machine. Nothing goes anywhere else. There is no server, no account, no API key, no analytics endpoint. The only internet activity that ever happens is a one-time Git clone on GNOME systems to set up a required shell extension, and that is a shell command the OS runs, not HPR itself.

## What HPR Is Not

HPR does not take screenshots. It does not log keystrokes. It does not record mouse movement. It does not read the contents of your windows. It reads exactly one thing: the name of the currently active application. That is the full scope of what it captures.

HPR is also not a subscription service, not a SaaS dashboard, and not an Electron app. It is a compiled C++ binary. It starts in milliseconds and uses under 30MB of RAM while running.

## What It Actually Shows You

At the moment HPR shows you three things in real time:

The name of the application you are currently in. The total time you have spent in each application today, shown in a human readable format like `2h 14m 30s`. The history of every application switch you made, showing which app you switched from, which app you switched to, and at what time the switch happened.

You can also load any past day by clicking the clock icon in the sidebar and picking a date from the date picker. HPR loads that day's SQLite file asynchronously and shows you exactly what you were doing.

This is the foundation. Analytics, insights, focus mode, and more are planned and described in the roadmap section below.

## Where Your Data Goes

HPR stores your data in SQLite database files organized like this:

```
~/.local/share/HPR/HPR_DB/ (Linux)
%APPDATA%\HPR\HPR_DB\ (Windows)
    05-26/
        01-05-26.db
```

One file per day. One folder per month. To delete everything from last month, delete the folder. To see exactly what was recorded on any specific day, open that day's `.db` file with any SQLite viewer. The files are completely standard SQLite, compatible with every SQLite tool ever made. A typical day of usage produces a file somewhere between 30 and 100 kilobytes. A full year of data is well under 50 megabytes.

## Installation

HPR does not have a formal installer yet. Download the latest release, extract it, and run the setup script once.

**Linux:**
```bash
chmod +x installHPRConfigAndUi.sh
./installHPRConfigAndUi.sh
./HPR
```

**Windows:**
```bat
installHPRConfigAndUi.bat
HPR.exe
```

The script copies `aliases.csv`, `config.csv`, and the `ui/` folder to the correct config directory for your platform. It will ask before overwriting any file you have already modified, so running it again after an update is safe. It also always copies the latest default UI into `ui-REFERENCEONLY/` so you always have a clean copy to diff against if something breaks after an update.

**Config locations after setup:**
```
Linux:   ~/.config/HPR/
Windows: %APPDATA%\HPR\HPR_Config\
```

**Data locations:**
```
Linux:   ~/.local/share/HPR/HPR_DB/
Windows: %APPDATA%\HPR\HPR_DB\
```

## Platform Support

HPR works on Hyprland (Linux, Wayland), GNOME (Linux, Wayland), KDE Plasma (Linux, Wayland/X11), and Windows 10 and 11.

On Hyprland, setup is zero effort. HPR queries the compositor directly using `hyprctl` and everything works on first launch.

On GNOME with Wayland, HPR requires a shell extension called `window-calls-extended` to expose the focused window information. On first launch, HPR checks whether the extension is already working. If it is not, HPR will prompt you to run the `installWindowCallsExtension.sh` script that ships next to the binary. That script clones and enables the extension for you. Because GNOME on Wayland cannot reload shell extensions without a session restart, you will need to log out and back in once after running it. After that one-time setup, subsequent launches work without any intervention.

On KDE Plasma, HPR uses KWin's scripting API via `qdbus6` to query the active window. No additional setup is required.

On Windows, HPR uses the Win32 API to query the active process name. No additional setup is required. The binary is built without a console window so it runs cleanly in the background.

## Performance

HPR is designed to stay out of your way.

RAM usage is under 30MB on all platforms. Windows sits around 8MB. GNOME and KDE sit around 20MB. Hyprland reports around 30MB in tools like BTOP++ but that number is misleading -- if you get an actual breakdown, HPR itself accounts for roughly 20MB and the rest is GPU library overhead that Hyprland attributes to HPR incorrectly. If it bothers you, turn off hardware acceleration in `config.csv` and it drops significantly. CPU usage is 1-3% on modern hardware during normal use. Startup is instant.

**Cachegrind (15 seconds):**
```
L1 instruction miss rate:          0.20%
Last-level instruction miss rate:  0.01%
L1 data miss rate:                 2.6%
Last-level data miss rate:         0.2%
Overall last-level miss rate:      ~0.0%
```

The 2.6% L1 data miss rate is entirely inside Slint's font rendering pipeline -- fontconfig querying, FreeType hinting, and parley text shaping. Nothing in HPR's own code shows up.

**Callgrind (60 seconds):**

Out of 4.46 billion total instructions executed, HPR's own C++ backend did not appear in the top 15 functions by instruction count. Everything in the top 15 was Slint, FreeType, fontconfig, or parley doing text rendering work. The background threads do their work and go to sleep. The main thread keeps the UI alive. That is how it is supposed to work.

## Privacy

- No accounts
- No telemetry
- No analytics
- No network communication of any kind

All data is stored locally in:

```
~/.local/share/HPR/HPR_DB/ (Linux)
%APPDATA%\HPR\HPR_DB\ (Windows)
```

You have full control over your data at all times.

## Comparison With Other Trackers

> Come on, a table like this increases your `users`.

| Feature | HPR | ActivityWatch | RescueTime | Toggl |
|---|---|---|---|---|
| Binary size | ~2 MB (excluding dynamic library) | 200 MB+ | Cloud app | Cloud app |
| RAM usage | ~22 MB & ~30 MB (Hyprland) | 200 MB+ | N/A | N/A |
| Requires account | No | No | Yes | Yes |
| Data leaves your machine | Never | Never | Yes | Yes |
| Auto-tracking | Yes | Yes | Yes | No |
| Wayland support | Yes | Partial | N/A | N/A |
| Requires running web server | No | Yes | No | No |
| Open source | Yes | Yes | No | No |
| Startup time | Instant | Several seconds | N/A | N/A |
| Free | Yes (premium planned) | Yes | Limited | Limited |

The closest honest comparison is ActivityWatch. ActivityWatch is a mature project with a full web dashboard, browser extensions, plugin ecosystem, and multiple years of development. HPR is early-stage and has none of those things yet. What HPR has that ActivityWatch does not is a significantly smaller footprint, native Wayland support from day one, no embedded web server, and no Python runtime. If you want a mature tool today, use ActivityWatch. If you want something that will eventually be faster and leaner, HPR is being built for that.

## Customizing the UI (Advanced Users)

HPR has an interpreted UI mode that lets you modify the look and feel of the application without touching a single line of C++ or recompiling the binary. By default HPR uses a pre-compiled UI for speed, but you can override this to use your own custom `.slint` files.

### 1. Enable Interpreted Mode

Edit your `config.csv`:

- **Linux**: `~/.config/HPR/config.csv`
- **Windows**: `%APPDATA%\HPR\HPR_Config\config.csv`

Add or change this line:
```csv
use-interpreter,true
```

### 2. Add Your UI Files

HPR looks for a file named `app-window.slint` in the following location:

- **Linux**: `~/.config/HPR/ui/`
- **Windows**: `%APPDATA%\HPR\HPR_Config\ui\`

The `installHPRConfigAndUi` script copies the default UI files there on first run. Edit them freely.

> [!IMPORTANT]
> When modifying the `.slint` files, you **must** keep the names of all structs, properties, and callbacks exactly as they are in the original file. HPR's C++ logic depends on these specific names to send data to the UI. As long as the names remain identical, you can change everything else -- colors, layouts, sizes, animations, component structure.

Read `READ_ME_BEFORE_MODIFYING_UI.txt` in your UI folder before touching anything. It explains exactly what you can and cannot rename, how data flows into your UI, and what the date picker format constraint is.

The `ui-REFERENCEONLY/` folder in your config directory always contains the latest default UI that shipped with the current version of HPR. If your changes break something, diff against it.

## Aliases

HPR ships with an `aliases.csv` that maps raw OS window names to human-readable labels. `code` matches `code`, `vscode`, `code-oss`, `code.exe`. `terminal` matches `kitty`, `alacritty`, `konsole`, `wezterm`, and so on.

You can add your own rules. The format is `raw substring,Display Name`. Lines starting with `#` are comments.

Changes are picked up automatically while HPR is running. No restart needed.

## Config

`config.csv` supports the following keys:

```csv
use-interpreter,false       # set to true to use custom .slint UI files
hardware-acceleration,true  # set to false if RAM reporting looks wrong on Hyprland
```

## Roadmap

The following is what is actually planned in roughly the order it will be built.

Human-readable insights derived entirely from code, no LLM involved. Things like most-used application today, longest uninterrupted focus session, total tracked time, and which application you switch away from most frequently. These are simple calculations on data HPR already has.

Historical session browser so you can look at past days without opening SQLite files manually. Currently you can load one day at a time via the sidebar date picker.

Data export to CSV and JSON.

A premium tier is planned for the future. The free tier will always include full local tracking, full data ownership, and the code-derived basic insights. The premium tier is intended to include LLM-powered pattern analysis that can read your usage data and give you personalized observations about your working patterns, focus mode with application blocking, advanced reporting, and browser tab tracking. This is not imminent.

---
---

# FOR DEVELOPERS AND POWER USERS

---

## Architecture Overview

HPR is a multi-threaded C++23 application built around a centralized shared state model. The threading architecture utilizes four distinct threads running concurrently. This design ensures the UI remains fully responsive while blocking I/O and polling operations occur asynchronously.

1. **Main Thread (UI Loop):** The main thread serves as the entry point in `main.cpp`. It instantiates the `ConfigManager` to load settings, then initializes the `DatabaseManager` and `CurrentWindowManager`. Depending on the configuration, it instantiates either the compiled `HPR` class or the dynamic `HPRInterpreter` class and enters the Slint event loop.

2. **Window Polling Thread:** Encapsulated within the `CurrentWindowManager` class. The `getCurrentWindow_Loop` method runs continuously, invoking the platform-specific active window getter every 50 milliseconds. It acquires `AppState::stateMutex` to update the current window name and accumulate elapsed time.

3. **UI Bridge and Model Management:** Handled by the `HPR` or `HPRInterpreter` classes in conjunction with `UiModelManager`. The `trackingLoop` wakes every 500 milliseconds, reads the raw C++ state, and uses `UiModelManager` to update the Slint-specific models in either compiled or interpreted mode. User interactions are handled by `UiEventBridge`, which translates Slint callbacks into internal application events via `EventHub`.

4. **Database Writer Thread:** Encapsulated within the `DatabaseManager` class in the `writeLoop` method. It wakes every 10 seconds to copy `AppState` data and flush it to a per-day SQLite database. It also listens for `LOAD_DATABASE_SINGULAR` events to asynchronously load historical data from past files.

## Shared State and Synchronization

All shared mutable data resides in a globally accessible namespace defined in `appState.hpp`:

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

The state is instantiated exactly once in `appState.cpp`. Every thread accessing this state must acquire `stateMutex` using `std::lock_guard<std::mutex>`. The application uses a coarse-grained locking strategy where the entire struct is locked simultaneously. Locks are acquired within scoped blocks `{}` to ensure the mutex is held for the absolute minimum duration. The pattern everywhere is: lock, copy, release immediately, then do all expensive work on the copy.

The `timeLog_PerApp` map accumulates raw duration in milliseconds. The `switchHistory` map uses a `std::pair<std::string, std::string>` as the key representing source and destination windows. The value is a `std::vector<uint64_t>` storing Unix millisecond timestamps for every recorded instance of that transition.

## Event System

HPR uses a centralized in-process publish-subscribe event bus (`EventHub`) for communication between decoupled components. The UI layer and database layer have no direct references to each other. They communicate entirely through typed events.

```cpp
// Subscribe
EventHub::connect(Event::HISTORY_LOADED_SINGULAR, [this](EventData data) { ... });

// Publish
EventHub::emit(Event::LOAD_DATABASE_SINGULAR, DatabaseDate_Singular{requestedDate});

// Cleanup
EventHub::disconnect(Event::HISTORY_LOADED_SINGULAR, id);
```

`EventData` is a `std::variant` so event payloads are type-safe. Subscribers get an ID on connect and use it to disconnect in their destructor.

## UI Bridging and Slint Interoperability

HPR supports two modes of UI execution, both managed by `UiModelManager`:

- **Compiled Mode (`HPR` class):** Slint compiles `app-window.slint` at build time into generated C++ code. Maximum performance, smallest deployment footprint.
- **Interpreted Mode (`HPRInterpreter` class):** The Slint interpreter loads `app-window.slint` dynamically from `~/.config/HPR/ui/` at runtime. Lets users modify the UI without recompiling anything.

`UiModelManager` abstracts the differences between these two modes, providing `update()` and `update_Interpreted()` methods that populate Slint `VectorModel`s from raw C++ data. Because Slint objects are not thread-safe, all UI updates are dispatched to the main thread via `slint::invoke_from_event_loop`.

The model update uses a surgical sync pattern instead of clearing and repopulating:

```cpp
// Update existing rows in place, remove excess, append new ones
// Clearing the model causes layout panics during resize/maximize
auto syncModel = [](auto model, const auto& vec) {
    size_t existing = model->row_count();
    size_t incoming = vec.size();
    size_t overlap  = std::min(existing, incoming);
    for (size_t i = 0; i < overlap; ++i)
        model->set_row_data(i, vec[i]);
    while (model->row_count() > incoming)
        model->erase(model->row_count() - 1);
    for (size_t i = existing; i < incoming; ++i)
        model->push_back(vec[i]);
};
```

## Database Layer

The database layer uses `sqlite_modern_cpp`, a header-only C++ wrapper over SQLite3. SQLite3 is compiled directly into the HPR binary from the official single-file amalgamation, so there are zero external system dependencies.

The schema uses two distinct persistence strategies:

- `app_usage`: `UNIQUE` constraint on `name`, `INSERT OR REPLACE`. Always one row per app, always the current total.
- `switch_history`: `UNIQUE` constraint on `timestamp`, `INSERT OR IGNORE`. The write loop dumps the entire history vector every flush. SQLite silently ignores duplicates. No diffing logic needed.

WAL mode is enabled on every database connection:

```sql
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
```

A passive WAL checkpoint runs after every write cycle. This was added after hitting real WAL corruption on Btrfs+LUKS. It is not theoretical.

The database writer thread sleeps in 100 chunks of 100ms rather than one 10-second sleep, so the application exits within 100ms of shutdown rather than waiting for the sleep to expire.

HPR also enforces single-instance behavior via a lock file:

- **Windows:** `CreateFileA` with `FILE_FLAG_DELETE_ON_CLOSE`. Auto-deletes even if HPR crashes.
- **Linux:** `flock` with `LOCK_EX | LOCK_NB`. Kernel releases the lock automatically when the process dies.

If HPR is already running, a second launch detects the lock and exits immediately.

## Timing Model

HPR enforces a strict separation between duration measurement and wall-clock timestamps:

- `std::chrono::steady_clock` is used exclusively to measure elapsed time between polling intervals. Monotonic, unaffected by NTP adjustments, DST changes, or the user changing their system clock.
- `std::chrono::system_clock` is used only to record when a window switch occurred, for display purposes.

Using the wrong clock for duration measurement is a common bug. A DST change or NTP adjustment mid-session would corrupt accumulated time if `system_clock` were used for duration. It is not.

## Window Name Normalization and Aliasing

Raw window titles from operating system APIs are inconsistent. `validateAndUpdateWindow_Cross` in `validateAndUpdateWindow.cpp` handles the first layer of normalization. It strips trailing newlines and carriage returns, lowercases for matching, and filters known noise:

```cpp
if (windowName.contains("searchhost")
    || windowName.contains("plasmashell")
    || windowName.contains("js::")       // KWin JS runtime artifact
    || windowName.contains("null")
    // ... etc
    )
    return "Unknown"; // skip this tick entirely
```

The `js::` filter specifically exists because the KDE backend briefly makes KWin's own JS runtime appear as the active window during injection. Without it, `js::kwin_tmp_1234` accumulates time in your log.

**Late-Binding Aliases:**

The database always stores raw OS strings. Aliases are applied at display time only, never at write time. This means renaming an alias retroactively updates all historical data without any migration. The raw data in SQLite is always preserved exactly as the OS reported it.

`AliasManager` does an O(N) substring scan through your `aliases.csv` rules the first time it sees a window name, then caches the result in an `unordered_map` for O(1) lookup on every subsequent query. Hot reload is supported -- save the file and HPR picks it up within the next UI tick.

## Class Lifecycle and Thread Management

Every class that manages a background thread follows the same lifecycle:

- Constructor allocates resources but does not start the thread.
- `run()` spawns the thread.
- The thread checks `std::atomic<bool> running` on every iteration.
- The destructor sets `running = false` and calls `join()` if `joinable()`.

This guarantees clean shutdown. The database writer completes its current flush before the process exits. Your data is safe even if you force-close HPR.

## Building From Source

**Requirements:**

- CMake 3.21 or later
- GCC 13+, Clang 16+, or MSVC 2022+ with C++23 support
- Slint 1.16.1 (installed via the provided script)
- On Linux: `jq` for Hyprland, `gdbus` for GNOME, `qdbus6` for KDE

**Install dependencies:**

```bash
git clone https://github.com/plexescor/HPR
cd HPR
sudo ./installDependencies.sh
```

On Windows, run `installDependencies.bat` instead.

This script downloads Slint 1.16.1, `slint-lsp`, and `slint-viewer` from GitHub releases and installs them to `/usr/local/`.

**Build:**

```bash
mkdir build && cd build
cmake ..
cmake --build . --parallel 8
```

SQLite is bundled as the official single-file amalgamation in `external/sqLite/` and compiled directly into the binary. No system SQLite packages are required.

The CMake build also copies all required runtime files next to the binary automatically: `aliases.csv`, `config.csv`, `ui/`, `assets/`, and the install scripts. A fresh build is immediately runnable.

## Adding a New Platform

Platform-specific window detection is isolated entirely within `CurrentWindowManager`. The constructor reads `$XDG_CURRENT_DESKTOP` on Linux and dispatches to the appropriate backend in `getCurrentWindow()`.

To add a new platform:

1. Declare `getCurrentWindow_YourPlatform()` in `getCurrentWindow.hpp`.
2. Implement it in `getCurrentWindow.cpp`. Return the raw active window title as a `std::string`. Return empty string for unknown/transitional states.
3. Add an `else if (currentPlatform.contains("YourPlatform"))` branch in `getCurrentWindow()`.
4. Add any initialization checks in the `CurrentWindowManager` constructor inside the appropriate preprocessor guard.

`validateAndUpdateWindow_Cross` runs immediately after the platform getter returns, so new backends do not need to implement their own normalization or filtering.

## Adding New Tracked Data

To track a new metric:

1. **Define in Slint:** Add a new property or struct to `app-window.slint` and a UI element to render it.
2. **Define in State:** Add the field to `AppState::AppState` in `appState.hpp`.
3. **Collect Data:** Update `getCurrentWindow_Loop()` to populate the new field inside the `std::lock_guard<std::mutex>` block.
4. **UI Bridge:** Pass the new field into `modelManager.update()` in `HPR.cpp` and `HPRInterpreter.cpp`. Use `syncModel` for any collection types to prevent layout panics during resize.
5. **Persistence:** Add the required table in `DatabaseManager::initDatabase` and update `writeLoop` to flush it.

## Known Issues and Limitations

**GNOME Extension Handling:** If `window-calls-extended` is absent, HPR sets the platform string to `GNOME_NO_EXTENSION` and returns a hardcoded instruction string from the polling loop until the extension is installed. HPR does not attempt to run the install script autonomously.

**KDE Backend Performance:** The KDE backend injects a temporary JavaScript payload into KWin via `qdbus6` and scrapes the system journal for the output. This triggers multiple shell invocations and disk I/O on every 50ms tick. Somehow it only uses around 1% CPU, which was surprising. Other approaches were tested and did not work. The README for the KDE backend is basically "this is a hack, it works, I tested it."

**Linux Platform Identification:** HPR reads `$XDG_CURRENT_DESKTOP` and matches with `std::string::contains`. Non-standard session variables or nested compositors may produce unexpected behavior.

## Contributing

The codebase is organized to be readable in a single session. Recommended reading order:

1. `main.cpp` -- initialization, config loading, thread spawning
2. `appState.hpp` -- the core shared data model
3. `getCurrentWindow.cpp` -- how window data is polled per platform
4. `databaseManager.cpp` -- persistence, WAL, instance lock, midnight rollover
5. `uiModelManager.cpp` -- how C++ data maps to Slint models in both compiled and interpreted mode

When contributing, make sure any access to shared state goes through `AppState::stateMutex`. Follow the copy-then-release pattern: lock, copy, release, then do the work on the copy.

There is no formal contribution process. Submit an issue or a pull request.

---

**Status:** Active development, v0.3
**Platforms:** Hyprland (Wayland), GNOME (Wayland), KDE Plasma (Wayland/X11), Windows 10/11
**Language:** C++23
**UI:** Slint 1.16.1
**Database:** SQLite3 (bundled amalgamation) via sqlite_modern_cpp