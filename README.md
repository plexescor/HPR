<p align="center">
  <a href="https://ko-fi.com/plexescor">
    <img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="Support on Ko-fi" />
  </a>
</p>

<p align="center">
  <img src="./assets/logo_1254png.png" alt="HPR Logo" width="300"/>
</p>

<h1 align="center">HPR &mdash; Human Pattern Recorder</h1>

<p align="center">
  <img src="https://img.shields.io/badge/status-active_development-brightgreen?style=flat-square" />
  <img src="https://img.shields.io/badge/version-v0.3-blue?style=flat-square" />
  <img src="https://img.shields.io/badge/language-C%2B%2B23-orange?style=flat-square" />
  <img src="https://img.shields.io/badge/UI-Slint_1.16.1-purple?style=flat-square" />
  <img src="https://img.shields.io/badge/DB-SQLite3_bundled-lightgrey?style=flat-square" />
  <img src="https://img.shields.io/badge/platforms-Windows_%7C_Linux-informational?style=flat-square" />
  <img src="https://img.shields.io/badge/telemetry-none-red?style=flat-square" />
</p>

<p align="center">
  A compiled, offline, zero-account activity tracker for Windows and Linux.<br/>
  Watches your active window. Logs your time. Stores everything locally. Talks to nothing.
</p>
<hr>
<p align="center">
  <img src="./assets/HPRHOME.png" alt="HPR Home Screen" width="800"/>
</p>
<hr>
<p align="center">
  <img src="./assets/HPRINSIGHTS.png" alt="HPR Insights Screen" width="800"/>
</p>
<hr>

## Table of Contents

- [What It Does](#what-it-does)
- [What It Does Not Do](#what-it-does-not-do)
- [Data Storage](#data-storage)
- [Installation](#installation)
- [Platform Support](#platform-support)
- [Performance](#performance)
- [Privacy](#privacy)
- [Comparison With Other Trackers](#comparison-with-other-trackers)
- [Aliases](#aliases)
- [Config](#config)
- [Customizing the UI](#customizing-the-ui-advanced)
- [Roadmap](#roadmap)
- [Architecture](#architecture-overview)
- [Shared State and Synchronization](#shared-state-and-synchronization)
- [Event System](#event-system)
- [UI Bridging and Slint Interoperability](#ui-bridging-and-slint-interoperability)
- [Database Layer](#database-layer)
- [Timing Model](#timing-model)
- [Window Name Normalization and Aliasing](#window-name-normalization-and-aliasing)
- [Class Lifecycle and Thread Management](#class-lifecycle-and-thread-management)
- [Building From Source](#building-from-source)
- [Adding a New Platform](#adding-a-new-platform)
- [Adding New Tracked Data](#adding-new-tracked-data)
- [Known Issues](#known-issues-and-limitations)
- [Contributing](#contributing)

---

## What It Does

HPR watches which window is active on your screen and builds a log of how much time you spend in each application, every day. Switch from Chrome to VSCode, it records that. Come back, it picks up where it left off. Close and reopen HPR the next day, your history from today is already loaded.

At launch, HPR shows you three things in real time:

- The name of the application you are currently in
- Total time spent in each application today, formatted as `2h 14m 30s`
- A full switch history: which app you left, which app you entered, and at what time

Click the clock icon in the sidebar, pick any date from the date picker, and HPR loads that day's SQLite file asynchronously. Every past day you used HPR is there.

Everything it records lives in a folder on your machine. Nothing goes anywhere else. No server. No account. No API key. No analytics endpoint. The only external network call that ever happens is a one-time `git clone` on GNOME systems to set up a required shell extension, and that is a shell command the OS runs, not HPR.

---

## What It Does Not Do

HPR does not take screenshots. It does not log keystrokes. It does not record mouse movement. It does not read the contents of your windows.

It reads exactly one thing: **the name of the currently active application.**

HPR is also not a subscription service, not a SaaS dashboard, and not an Electron app. It is a compiled C++ binary. Starts in milliseconds. Uses under 50MB of RAM on Linux, under 10MB on Windows.

---

## Data Storage

HPR stores your data in SQLite databases organized by day:

```
~/.local/share/HPR/HPR_DB/          (Linux)
%APPDATA%\HPR\HPR_DB\               (Windows)

    05-26/
        01-05-26.db
        02-05-26.db
        ...
```

One file per day. One folder per month. To nuke everything from last month, delete the folder. To inspect any specific day, open the `.db` file in any SQLite viewer. The files are completely standard SQLite3, compatible with every tool ever made.

A typical day of usage: **30 to 100 KB**. A full year of data: **well under 50 MB**.

---

## Installation

HPR does not have a formal installer yet. Download the latest release, extract it, run the setup script once.

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

The script copies `aliases.csv`, `config.csv`, and the `ui/` folder to the correct config directory for your platform. It asks before overwriting anything you have already modified, so running it again after an update is safe. It also always copies the latest default UI into `ui-REFERENCEONLY/` so you have a clean reference to diff against if something breaks after an update.

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

---

## Platform Support

| Platform | Backend | Setup Required |
|---|---|---|
| Hyprland (Wayland) | `hyprctl` IPC | None. Works on first launch. |
| GNOME (Wayland) | `window-calls-extended` extension | One-time: run `installWindowCallsExtension.sh`, log out, log back in. |
| KDE Plasma (Wayland/X11) | KWin scripting via `qdbus6` | None. |
| Windows 10/11 | Win32 API | None. |

**GNOME note:** On first launch, HPR checks whether `window-calls-extended` is working. If not, it prompts you to run the bundled install script. That script clones and enables the extension. Because GNOME on Wayland cannot reload shell extensions without a session restart, you log out and back in once. After that, every subsequent launch is automatic.

**Windows note:** The binary is built without a console window so it runs cleanly in the background.

---

## Performance

<details>
<summary><strong>Windows</strong></summary>

HPR sits around **8 MB RSS**. That number is real and reflects only HPR's own code, data, and the minimal runtime libraries Windows loads.

</details>

<details>
<summary><strong>Linux (read this before filing a memory issue)</strong></summary>

Tools like BTOP++, htop, and similar process monitors will report approximately **47 MB**. That number is not wrong, but it is misleading without context.

HPR's own private memory on Linux -- heap, stack, all of your actual application data -- is roughly **22 MB**. The remaining ~25 MB is shared GPU libraries that the OS maps into HPR's address space because Slint's renderer uses OpenGL for hardware-accelerated drawing.

The chain: Slint initializes OpenGL via EGL -> OS loads Mesa's Gallium driver for your GPU -> Mesa's Gallium driver unconditionally loads `libLLVM` because it uses LLVM as a JIT compiler for shader code generation at the driver level. This happens even when all shaders are already compiled and cached. `libLLVM` is a 60 MB+ library and its resident pages alone account for roughly 26 MB of what process monitors report as HPR's memory.

**This is not a memory leak.** It is not HPR being inefficient. It is Mesa's architecture. Your compositor, your browser, and every other GPU-accelerated application on the system already have these same pages loaded. Because they are shared, the kernel maps the same physical memory pages into each process. The proportional cost to HPR is around **50 MB PSS** (Proportional Set Size), which is the most honest single number you can get from the kernel.

To verify this yourself:

```bash
cat /proc/$(pgrep HPR)/status | grep -E "RssAnon|RssFile|VmRSS"
```

`RssAnon` is HPR's actual private footprint. `RssFile` is shared library pages. `VmRSS` is the sum that process monitors display.

To eliminate the GPU library overhead entirely, disable hardware acceleration in `config.csv`:

```csv
hardware-acceleration,false
```

This switches Slint to a CPU software renderer. Mesa and `libLLVM` are never loaded. Total RSS drops to approximately **22 MB**. The UI looks identical. The only tradeoff is slightly higher CPU usage during redraws -- for a background tracker that redraws every 500ms, this is completely imperceptible.

</details>

<details>
<summary><strong>Cachegrind (15 seconds)</strong></summary>

```
L1 instruction miss rate:          0.20%
Last-level instruction miss rate:  0.01%
L1 data miss rate:                 2.6%
Last-level data miss rate:         0.2%
Overall last-level miss rate:      ~0.0%
```

The 2.6% L1 data miss rate is entirely inside Slint's font rendering pipeline: fontconfig querying, FreeType hinting, and parley text shaping. Nothing in HPR's own code shows up.

</details>

<details>
<summary><strong>Callgrind (60 seconds)</strong></summary>

Out of 4.46 billion total instructions executed, HPR's own C++ backend did not appear in the top 15 functions by instruction count. Everything in the top 15 was Slint, FreeType, fontconfig, or parley doing text rendering work. The background threads do their work and go to sleep. The main thread keeps the UI alive. That is how it is supposed to work.

</details>

CPU usage is 1-3% on modern hardware during normal use. Startup is instant.

---

## Privacy

```
No accounts.
No telemetry.
No analytics.
No network communication of any kind.
```

All data is stored locally. You have full control over your data at all times. Deleting a file or folder is the complete and total extent of what "removing your data" means.

---

## Comparison With Other Trackers

| Feature | HPR | ActivityWatch | RescueTime | Toggl |
|---|---|---|---|---|
| Binary size | ~2 MB | 200 MB+ | Cloud app | Cloud app |
| RAM usage | ~22 MB private / ~47 MB reported (Linux), ~8 MB (Windows) | 200 MB+ | N/A | N/A |
| Requires account | No | No | Yes | Yes |
| Data leaves your machine | Never | Never | Yes | Yes |
| Auto-tracking | Yes | Yes | Yes | No |
| Wayland support | Yes | Partial | N/A | N/A |
| Requires running web server | No | Yes | No | No |
| Open source | Yes | Yes | No | No |
| Startup time | Instant | Several seconds | N/A | N/A |
| Free | Yes (premium planned) | Yes | Limited | Limited |

The closest honest comparison is ActivityWatch. ActivityWatch is a mature project with a full web dashboard, browser extensions, plugin ecosystem, and multiple years of development. HPR is early-stage and has none of those things yet. What HPR has that ActivityWatch does not: significantly smaller footprint, native Wayland support from day one, no embedded web server, no Python runtime.

If you want a mature tool today, use ActivityWatch. If you want something that will eventually be faster and leaner, HPR is being built for that.

---

## Aliases

HPR ships with an `aliases.csv` that maps raw OS window names to human-readable labels:

- `code` matches `code`, `vscode`, `code-oss`, `code.exe`
- `terminal` matches `kitty`, `alacritty`, `konsole`, `wezterm`, and so on

Add your own rules. Format: `raw substring,Display Name`. Lines starting with `#` are comments.

**Changes are picked up automatically while HPR is running. No restart needed.**

---

## Config

`config.csv` supports the following keys:

```csv
use-interpreter,false       # set to true to use custom .slint UI files
hardware-acceleration,true  # set to false to eliminate GPU library overhead on Linux
```

Config locations:
```
Linux:   ~/.config/HPR/config.csv
Windows: %APPDATA%\HPR\HPR_Config\config.csv
```

---

## Customizing the UI (Advanced)

HPR has an interpreted UI mode that lets you modify the look and feel without touching C++ or recompiling. By default HPR uses a pre-compiled UI, but you can override it to use your own `.slint` files.

**Step 1: Enable interpreted mode**

```csv
# config.csv
use-interpreter,true
```

**Step 2: Edit your UI files**

HPR looks for `app-window.slint` at:
```
Linux:   ~/.config/HPR/ui/
Windows: %APPDATA%\HPR\HPR_Config\ui\
```

The `installHPRConfigAndUi` script copies the default UI files there on first run. Edit them freely.

> [!IMPORTANT]
> You **must** keep the names of all structs, properties, and callbacks exactly as they are in the original file. HPR's C++ logic depends on these specific names to send data to the UI. Change colors, layouts, sizes, animations, component structure freely. Do not rename the interface contracts.

Read `READ_ME_BEFORE_MODIFYING_UI.txt` in your UI folder before touching anything. It explains exactly what you can and cannot rename, how data flows into your UI, and the date picker format constraint.

The `ui-REFERENCEONLY/` folder in your config directory always contains the latest default UI that shipped with the current version. If your changes break something, diff against it.

---

## Roadmap

> [!NOTE]
> The core vision for HPR's free version is now **mostly complete**.
> The major foundational goals -- local-first tracking, privacy-focused architecture, detailed usage analytics, lightweight native performance, and fully offline data ownership -- have already been implemented or are nearing completion.
>
> Future free-tier development will focus on refinement, polish, stability, UI/UX improvements, and smaller quality-of-life features rather than massive missing functionality.

**Free tier (always):** Full local tracking, full data ownership, code-derived basic insights.

**Premium tier (planned, not imminent):** LLM-powered pattern analysis, focus mode with application blocking, advanced reporting, browser tab tracking.

---

---

# For Developers and Power Users

---

## Architecture Overview

HPR is a multi-threaded C++23 application built around a centralized shared state model. Four distinct threads run concurrently:

```
Main Thread (UI Loop)
    |-- Window Polling Thread       [50ms tick, CurrentWindowManager]
    |-- UI Bridge / Model Manager  [500ms tick, HPR / HPRInterpreter + UiModelManager]
    |-- Database Writer Thread     [10s tick + event-driven, DatabaseManager]
```

**Main Thread:** Entry point in `main.cpp`. Instantiates `ConfigManager`, `DatabaseManager`, and `CurrentWindowManager`. Depending on config, instantiates either the compiled `HPR` class or the dynamic `HPRInterpreter` class, then enters the Slint event loop.

**Window Polling Thread:** Lives inside `CurrentWindowManager`. `getCurrentWindow_Loop` runs continuously, invoking the platform-specific active window getter every 50ms. Acquires `AppState::stateMutex` to update the current window name and accumulate elapsed time.

**UI Bridge and Model Management:** Handled by `HPR` or `HPRInterpreter` in conjunction with `UiModelManager`. The `trackingLoop` wakes every 500ms, reads the raw C++ state, and uses `UiModelManager` to update Slint-specific models in either compiled or interpreted mode. User interactions are handled by `UiEventBridge`, which translates Slint callbacks into internal application events via `EventHub`.

**Database Writer Thread:** Lives inside `DatabaseManager` in the `writeLoop` method. Wakes every 10 seconds to flush `AppState` data to a per-day SQLite database. Also listens for `LOAD_DATABASE_SINGULAR` events to asynchronously load historical data.

---

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

The state is instantiated exactly once in `appState.cpp`. Every thread accessing this state must acquire `stateMutex` via `std::lock_guard<std::mutex>`. The application uses a coarse-grained locking strategy where the entire struct is locked simultaneously. Locks are acquired within scoped blocks `{}` to ensure the mutex is held for the absolute minimum duration. The pattern everywhere:

```
lock -> copy -> release immediately -> do all expensive work on the copy
```

`timeLog_PerApp` accumulates raw duration in milliseconds. `switchHistory` uses a `std::pair<std::string, std::string>` as the key representing source and destination windows. The value is a `std::vector<uint64_t>` storing Unix millisecond timestamps for every recorded instance of that transition.

---

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

`EventData` is a `std::variant`, so event payloads are type-safe. Subscribers get an ID on connect and use it to disconnect in their destructor.

---

## UI Bridging and Slint Interoperability

HPR supports two modes of UI execution, both managed by `UiModelManager`:

| Mode | Class | How |
|---|---|---|
| Compiled | `HPR` | Slint compiles `app-window.slint` at build time into generated C++. Maximum performance, smallest deployment footprint. |
| Interpreted | `HPRInterpreter` | The Slint interpreter loads `app-window.slint` dynamically from `~/.config/HPR/ui/` at runtime. Modify the UI without recompiling anything. |

`UiModelManager` abstracts the differences, providing `update()` and `update_Interpreted()` methods that populate Slint `VectorModel`s from raw C++ data. Because Slint objects are not thread-safe, all UI updates are dispatched to the main thread via `slint::invoke_from_event_loop`.

The model update uses a surgical sync pattern instead of clearing and repopulating. Clearing the model causes layout panics during resize/maximize:

```cpp
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

---

## Database Layer

The database layer uses `sqlite_modern_cpp`, a header-only C++ wrapper over SQLite3. SQLite3 is compiled directly into the HPR binary from the official single-file amalgamation. Zero external system dependencies.

**Schema persistence strategies:**

```
app_usage      UNIQUE on name    -> INSERT OR REPLACE  -> always one row per app, always current total
switch_history UNIQUE on timestamp -> INSERT OR IGNORE -> dump full history on every flush, SQLite silently skips duplicates, no diffing needed
```

**WAL mode on every connection:**

```sql
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
```

A passive WAL checkpoint runs after every write cycle. This was added after hitting real WAL corruption on Btrfs+LUKS. It is not theoretical.

The database writer thread sleeps in 100 chunks of 100ms rather than one 10-second sleep, so the application exits within 100ms of shutdown rather than waiting for the full sleep to expire.

**Single-instance enforcement via lock file:**

- **Windows:** `CreateFileA` with `FILE_FLAG_DELETE_ON_CLOSE`. Auto-deletes even if HPR crashes.
- **Linux:** `flock` with `LOCK_EX | LOCK_NB`. Kernel releases the lock automatically when the process dies.

If HPR is already running, a second launch detects the lock and exits immediately.

---

## Timing Model

HPR enforces a strict separation between duration measurement and wall-clock timestamps:

| Clock | Used for |
|---|---|
| `std::chrono::steady_clock` | Measuring elapsed time between polling intervals. Monotonic, unaffected by NTP adjustments, DST changes, or the user changing their system clock. |
| `std::chrono::system_clock` | Recording when a window switch occurred, for display purposes only. |

Using `system_clock` for duration measurement is a common bug. A DST change or NTP adjustment mid-session would corrupt accumulated time. HPR does not do this.

---

## Window Name Normalization and Aliasing

Raw window titles from OS APIs are inconsistent. `validateAndUpdateWindow_Cross` in `validateAndUpdateWindow.cpp` handles the first layer of normalization: strips trailing newlines and carriage returns, lowercases for matching, and filters known noise:

```cpp
if (windowName.contains("searchhost")
    || windowName.contains("plasmashell")
    || windowName.contains("js::")       // KWin JS runtime artifact
    || windowName.contains("null")
    // ...
    )
    return "Unknown"; // skip this tick entirely
```

The `js::` filter exists because the KDE backend briefly makes KWin's own JS runtime appear as the active window during injection. Without it, `js::kwin_tmp_1234` accumulates time in your log.

**Late-Binding Aliases:**

The database always stores raw OS strings. Aliases are applied at display time only, never at write time. This means renaming an alias retroactively updates all historical data without any migration. The raw data in SQLite is always preserved exactly as the OS reported it.

`AliasManager` does an O(N) substring scan through your `aliases.csv` rules the first time it sees a window name, then caches the result in an `unordered_map` for O(1) lookup on every subsequent query. Hot reload is supported: save the file and HPR picks it up within the next UI tick.

---

## Class Lifecycle and Thread Management

Every class that manages a background thread follows the same lifecycle:

```
Constructor     -> allocates resources, does NOT start the thread
run()           -> spawns the thread
thread body     -> checks std::atomic<bool> running on every iteration
destructor      -> sets running = false, calls join() if joinable()
```

This guarantees clean shutdown. The database writer completes its current flush before the process exits. Your data is safe even if you force-close HPR.

---

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

On Windows, run `installDependencies.bat` instead. The script downloads Slint 1.16.1, `slint-lsp`, and `slint-viewer` from GitHub releases and installs them to `/usr/local/`.

**Build:**

```bash
mkdir build && cd build
cmake ..
cmake --build . --parallel 8
```

SQLite is bundled as the official single-file amalgamation in `external/sqLite/` and compiled directly into the binary. No system SQLite packages are required. The CMake build also copies all required runtime files next to the binary automatically: `aliases.csv`, `config.csv`, `ui/`, `assets/`, and the install scripts. A fresh build is immediately runnable.

---

## Adding a New Platform

Platform-specific window detection is isolated entirely within `CurrentWindowManager`. The constructor reads `$XDG_CURRENT_DESKTOP` on Linux and dispatches to the appropriate backend in `getCurrentWindow()`.

1. Declare `getCurrentWindow_YourPlatform()` in `getCurrentWindow.hpp`
2. Implement it in `getCurrentWindow.cpp`. Return the raw active window title as a `std::string`. Return empty string for unknown/transitional states.
3. Add an `else if (currentPlatform.contains("YourPlatform"))` branch in `getCurrentWindow()`
4. Add any initialization checks in the `CurrentWindowManager` constructor inside the appropriate preprocessor guard

`validateAndUpdateWindow_Cross` runs immediately after the platform getter returns, so new backends do not need to implement their own normalization or filtering.

---

## Adding New Tracked Data

1. **Define in Slint:** Add a new property or struct to `app-window.slint` and a UI element to render it
2. **Define in State:** Add the field to `AppState::AppState` in `appState.hpp`
3. **Collect Data:** Update `getCurrentWindow_Loop()` to populate the new field inside the `std::lock_guard<std::mutex>` block
4. **UI Bridge:** Pass the new field into `modelManager.update()` in `HPR.cpp` and `HPRInterpreter.cpp`. Use `syncModel` for any collection types to prevent layout panics during resize.
5. **Persistence:** Add the required table in `DatabaseManager::initDatabase` and update `writeLoop` to flush it

---

## Known Issues and Limitations

> [!WARNING]
> **GNOME Extension Handling:** If `window-calls-extended` is absent, HPR sets the platform string to `GNOME_NO_EXTENSION` and returns a hardcoded instruction string from the polling loop until the extension is installed. HPR does not attempt to run the install script autonomously.

> [!WARNING]
> **KDE Backend Performance:** The KDE backend injects a temporary JavaScript payload into KWin via `qdbus6` and scrapes the system journal for the output. This triggers multiple shell invocations and disk I/O on every 50ms tick. It somehow only uses around 1% CPU, which was surprising. Other approaches were tested and did not work. This is a hack. It works. It has been tested.

> [!NOTE]
> **Linux Platform Identification:** HPR reads `$XDG_CURRENT_DESKTOP` and matches with `std::string::contains`. Non-standard session variables or nested compositors may produce unexpected behavior.

---

## Contributing

The codebase is organized to be readable in a single session. Recommended reading order:

```
main.cpp                  ->  initialization, config loading, thread spawning
appState.hpp              ->  the core shared data model
getCurrentWindow.cpp      ->  how window data is polled per platform
databaseManager.cpp       ->  persistence, WAL, instance lock, midnight rollover
uiModelManager.cpp        ->  how C++ data maps to Slint models in compiled and interpreted mode
```

When contributing: any access to shared state goes through `AppState::stateMutex`. Follow the copy-then-release pattern: lock, copy, release, then do the work on the copy.

No formal contribution process. Submit an issue or a pull request.

---

<p align="center">
  <sub>
    Active development &nbsp;|&nbsp; v0.3 &nbsp;|&nbsp;
    Platforms: Hyprland (Wayland), GNOME (Wayland), KDE Plasma (Wayland/X11), Windows 10/11 &nbsp;|&nbsp;
    Language: C++23 &nbsp;|&nbsp; UI: Slint 1.16.1 &nbsp;|&nbsp; DB: SQLite3 (bundled amalgamation) via sqlite_modern_cpp
  </sub>
</p>