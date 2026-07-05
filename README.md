<p align="center">
  <a href="https://ko-fi.com/plexescor">
    <img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="Support on Ko-fi" />
  </a>
</p>

> HPR is built solo by a 16-year-old developer in India. My Dad is pushing me to focus on JEE -- India's national engineering entrance exam, one of the most brutally competitive exams in the world -- so HPR development happens in whatever time I can steal from that. If I were an adult I would not have made HPR free at all because I would've needed money to survive, but I am a teen and I live under my Dad so I could "afford" HPR to be fully free and open source. If donations are enough to show this is worth continuing, I can justify spending time on HPR and try my hand at other things instead of just grinding JEE prep. If HPR has been useful to you, a Ko-fi donation genuinely helps.

---

## Sponsors & Supporters

A huge thank you to our supporters keeping HPR active!

- **[Jesse Kramer](https://ko-fi.com/jessekramer)** ($20) -- *First Supporter* 💖

---

> **On Slint 1.17 and tray support:** I'm aware Slint 1.17 shipped with built-in tray icon support. I'm not upgrading to it. Slint's tray implementation doesn't work reliably across all platforms HPR targets -- Waybar in particular has no usable support. Until Slint's tray story improves across the board, I'm keeping my native `libdbus-1` / Win32 approach which I know works everywhere HPR runs.

---

<p align="center">
  <img src="./assets/logo_1254png.png" alt="HPR Logo" width="300"/>
</p>

<h1 align="center">HPR &mdash; Human Pattern Recorder</h1>

<p align="center">
  <img src="https://img.shields.io/badge/status-active_development-brightgreen?style=flat-square" />
  <img src="https://img.shields.io/badge/version-v0.9.3-blue?style=flat-square" />
  <img src="https://img.shields.io/badge/language-C%2B%2B23-orange?style=flat-square" />
  <img src="https://img.shields.io/badge/UI-Slint_1.16.1-purple?style=flat-square" />
  <img src="https://img.shields.io/badge/DB-SQLite3_bundled-lightgrey?style=flat-square" />
  <img src="https://img.shields.io/badge/platforms-Windows_%7C_Linux-informational?style=flat-square" />
  <img src="https://img.shields.io/badge/telemetry-opt--in_only-yellow?style=flat-square" />
</p>

<p align="center">
  <strong>A compiled, offline, zero-account activity tracker.</strong><br/>
  Watches your active window. Builds your history. Never phones home.
</p>

---

<p align="center">
  <em>"HPR is an excellent tool for time management on Linux! It far outweighs any other option available and is developing very quickly! I highly recommend it!"</em><br/>
  <sub>-- <a href="https://github.com/dotsupershow">@dotsupershow</a>, Niri user</sub>
</p>

---

> [!IMPORTANT]
> **HPR is fully free and open source.**
> Every feature -- current and future -- is available to everyone at no cost. If HPR saves you time or you want to support continued development, a Ko-fi donation goes a long way.

---

<p align="center">
  <img src="./assetsgithub/main.png" alt="HPR Home Screen" width="800"/>
</p>

---

<p align="center">
  <img src="./assetsgithub/insights.png" alt="HPR Insights Screen" width="800"/>
</p>

---

<p align="center">
  <strong>See HPR in action</strong><br/>
  <sub>Live window tracking, switch history, and the Insights engine -- all running locally, zero accounts.</sub>
</p>

<p align="center">

https://github.com/user-attachments/assets/07659d3d-0f3b-4bbc-8823-8b5d11bfd32f

</p>

---

## Table of Contents

- [What It Does](#what-it-does)
- [Browser Tab Tracking](#browser-tab-tracking)
- [VS Code Project Tracking](#vs-code-project-tracking)
- [App Limits and Goals](#app-limits-and-goals)
- [Day Construction Timeline](#day-construction-timeline)
- [Advanced Pattern Analysis](#advanced-pattern-analysis)
- [Theme Management](#theme-management)
- [Extensions](#extensions)
- [System Tray](#system-tray)
- [Data Storage](#data-storage)
- [Installation](#installation)
- [Platform Support](#platform-support)
- [Performance](#performance)
- [Privacy](#privacy)
- [Comparison With Other Trackers](#comparison-with-other-trackers)
- [Aliases](#aliases)
- [Config](#config)
- [Customizing the UI](#custom-themes--customizing-the-ui-advanced)
- [Roadmap](#roadmap)
- [Architecture Overview](#architecture-overview)
- [Shared State and Synchronization](#shared-state-and-synchronization)
- [Event System](#event-system)
- [UI Bridging and Slint Interoperability](#ui-bridging-and-slint-interoperability)
- [Database Layer](#database-layer)
- [Timing Model](#timing-model)
- [Pattern Analysis Engine](#pattern-analysis-engine)
- [Window Name Normalization and Aliasing](#window-name-normalization-and-aliasing)
- [Class Lifecycle and Thread Management](#class-lifecycle-and-thread-management)
- [Building From Source](#building-from-source)
- [Adding a New Platform](#adding-a-new-platform)
- [Adding New Tracked Data](#adding-new-tracked-data)
- [Known Issues and Limitations](#known-issues-and-limitations)
- [Contributing](#contributing)

---

## What It Does

You open your computer. You work. Hours pass. You have no idea where they went.

HPR fixes that. It watches which window is in focus every 50 milliseconds, all day. It builds a running log of exactly where your time actually went -- not where you think it went. Switch from your browser to your editor, it records the transition. Switch back two hours later, it records that too. Every switch. Every minute. Every day.

At any point you get three things live:

- **What you are in right now**, updating in real time
- **Total time per application today**, displayed as `2h 14m 30s`
- **Your complete switch history**: every transition, timestamped, in order

**Historical data -- three modes:**

Click the calendar icon in the sidebar to open the History Range view:

| Mode | What it does |
|---|---|
| **Single Day** | Pick any specific date. Loads that day's `.db` file asynchronously off disk. |
| **Last N Days** | Pull last 7, 14, 30 days or any custom count. Multiple daily files are merged and aggregated. |
| **Date Range** | Set a start and end date. HPR reads every daily file in the span and streams the combined result back. |

Historical loading runs on N number of threads where N = Days (no pool) -- live tracking is never paused. A "Switch to Live View" button in the data view takes you back to real-time instantly.

<p align="center">
  <img src="./assetsgithub/history.png" alt="HPR History Range View" width="800"/>
</p>

**Tray controls:**

| Action | Windows | Linux (Waybar / KDE / Cinnamon) |
|---|---|---|
| Open HPR | Right click > **Show HPR** | Left or right click |
| Quit HPR | Right click > **Quit** | Middle click |

> [!NOTE]
> On Linux, hovering over the tray icon shows **"HPR - Human Pattern Recorder"** as the tooltip title and **"Left/Right click: Open HPR | Middle click: Quit"** as the description.

---

## Browser Tab Tracking

HPR supports tracking browser tabs per site and per tab without requiring any browser extensions. When the active window is a supported browser (Chrome, Edge, Firefox, or Brave), HPR automatically queries the window title alongside the application name. This tab usage time is aggregated and tracked separately, giving you a detailed breakdown of which websites and tabs you spend time on.

In the UI, toggle display mode with the **Tab View** and **Site View** buttons:
- **Tab View**: Shows raw, unaliased tab names -- lets you differentiate between specific pages.
- **Site View**: Applies rules from `tabAliases.csv` to group tabs by website, collapsing specific pages into parent domains.

---

## VS Code Project Tracking

HPR tracks which VS Code project you are in, not just that VS Code is open. No extension required. No VS Code plugin to install.

VS Code puts the active project name directly in its window title in the format `filename - project - Visual Studio Code`. HPR reads that title on every poll tick and parses it:

1. Strip the trailing ` - Visual Studio Code` suffix
2. Find the last ` - ` separator in what remains
3. Everything after that separator is the project name

The result goes into `timeLog_PerProject`, a separate time accumulator running in parallel with the normal per-app log. The UI has a dedicated Project View showing time broken down by project name for the day. Toggle between **Raw View** (unprocessed title substring) and the default parsed view which applies `projectAliases.csv`.

This works on every supported platform because each backend already has a window title getter and VS Code puts the project name in the title on all of them.

---

## App Limits and Goals

Set a daily time **limit** or daily usage **goal** on any tracked application directly from the Goals view in the sidebar.

**Limits** cap an app to a maximum number of minutes per day. When usage crosses that threshold:
1. HPR sends a system notification.
2. Optionally, HPR can **force-quit the application automatically**.

**Goals** set a minimum number of minutes you want to spend in an app each day. HPR tracks progress and notifies you when you hit it.

**How to configure:**
1. Click the Goals icon in the sidebar -- every tracked app appears automatically.
2. Click any app row to expand it inline -- a minute input with +/- step buttons appears alongside three actions: **SET LIMIT**, **SET GOAL**, **RESET**.
3. Done. A dedicated background thread (`LimitsManager`) monitors usage continuously. No restart required.

Badges on each row show **remaining time live**. Limit rows have a red accent; goal rows have green.

<p align="center">
  <img src="./assetsgithub/limits.png" alt="HPR App Limits and Goals" width="800"/>
</p>

> [!NOTE]
> Advanced users can intercept the limit-reached function call via the [Function Overriding API](https://hpr-cpp.netlify.app/overrides.html) to run custom Lua logic -- log it, send a webhook, suppress the notification, or anything else.

---

## Day Construction Timeline

Rebuild your daily narrative with a visual, contiguous timeline of your system activity. Instead of just looking at raw accumulated numbers, the **Day Construction Timeline** maps your active window transitions chronologically onto a zoomable, scrollable canvas.

<p align="center">
  <img src="./assetsgithub/timeline.png" alt="HPR Day Construction Timeline" width="800"/>
</p>

**Key timeline features:**

- **Scrollable & Zoomable Viewports**: Focus on specific intervals or view the entire day. Click presets to zoom into `1h`, `3h`, or `8h` views, or zoom out to `24h` and `All Day` spans.
- **Adaptive Hour Markers**: Axis markers dynamically adjust spacing based on zoom level. Zoomed modes place markers every hour; high-span modes space them every 3 hours to prevent overlapping text.
- **Continuity Gap Capping**: If HPR is closed or your computer is off for a period, a naive timeline would stretch the last active app across the entire gap. HPR detects focus gaps and caps the preceding active segment to a maximum of 1 minute upon transitioning to Unknown states.
- **Live HPR Tracking**: HPR tracks itself as an application, giving you visibility into time spent configuring goals or managing extensions.
- **Hover Micro-interactions**: Hover over any timeline block to view its details. The status panel updates with the application's aliased name, exact active duration, and start/end time range.

---

## Advanced Pattern Analysis

Go beyond basic daily statistics with HPR's multi-day correlation engine. Under the **Insights** tab, HPR processes your historical database files to extract **9 advanced cross-day metrics**:

1. **Escape Pattern**: Average daily switches from your primary work application to distraction/browser apps.
2. **Return Rate**: Percentage of times you immediately bounce back to your work app after a browser escape.
3. **Average Focus Session**: Average duration of uninterrupted focus stretches before switching windows.
4. **Most Distracted Day**: The day of the week with the highest average multitasking frequency across all logged data.
5. **Productive Days**: Days during the current week where your hourly switch frequency stayed below the focus threshold.
6. **Screen Time vs Average**: Today's cumulative screen time compared against your N-day historical average.
7. **Focus Dip Hour**: The hour of the day where your switches spike most consistently across days.
8. **Deep Work Before Noon**: Percentage of days where your longest focus session began before 12:00 PM local time.
9. **Weekend vs Weekday**: Percentage difference in work-app usage between weekdays and weekends.

> [!NOTE]
> Work and browser applications are automatically resolved from your historical data or can be explicitly overridden in `config.csv`.

---

## Theme Management

HPR supports external themes loaded dynamically at runtime. No recompilation needed.

Themes live in:
- **Linux**: `~/.config/HPR/themes/`
- **Windows**: `%APPDATA%\HPR\HPR_Config\themes\`

Each theme is a subfolder containing an `app-window.slint` (the layout), an required `metadata.csv` (name, version info), and up to 9 preview images (`1.png` through `9.png`).

The **Themes View** in the sidebar shows all discovered themes with a horizontal preview carousel, description, version info, and apply/refresh controls. Applying a theme reloads the Slint component live with no restart. Selecting **default** reverts to the built-in layout.

> [!WARNING]
> Theme management requires `use-interpreter,true` in `config.csv`. Interpreter mode increases RAM usage by 20-50% due to Slint's runtime compiler and decoded preview image buffers.

For a starting point, download the [Minimal Boilerplate Theme](https://hpr-cpp.netlify.app/assets/boilerplate-theme.zip). Full theme authoring guide at [hpr-cpp.netlify.app/themes.html](https://hpr-cpp.netlify.app/themes.html).

---

## Extensions

HPR ships with a built-in **Sandboxed Lua 5.4 Extension Engine**. Drop a `.lua` file into your extensions folder and HPR loads it automatically. No compilers, no package managers, no boilerplate.

Each extension runs in its own isolated VM on a dedicated background thread, completely decoupled from the main tracking and rendering pipelines. A slow or misbehaving extension cannot block HPR's core loop or freeze the UI.

**What you can do with extensions:**
- Read the currently active window in real time
- Run shell commands and query the HPR database directly
- Register fully custom window detection backends for compositors HPR doesn't natively support
- Subscribe to HPR's internal event bus and react to state changes
- Build interactive custom UI panels that render inside HPR via Slint callback bindings
- **Function Overriding (Advanced)**: Intercept, block, or modify 26+ core C++ functions directly from Lua
- AND yes, you can run [Budget Doom](https://github.com/plexescor/HPR-Extensions) (software rendering only)

<p align="center">
  <img src="./assetsgithub/extensions.png" alt="HPR Extensions" width="800"/>
</p>

**Where to put your extensions:**

```
Linux:   ~/.config/HPR/extensions/
Windows: %APPDATA%\HPR\HPR_Config\extensions\
```

HPR scans recursively, so subdirectory organization like `extensions/my-backend/sway.lua` works fine.

**A minimal extension** -- prints the active app every tick:
```lua
function onTick(delta)
    print(HPR.getCurrentWindow_E())
end
```

**Extension lifecycle hooks:**

| Hook | When it runs |
|---|---|
| `init()` | Once on load. Return an integer to set tick interval in ms (default 1000). |
| `onTick(delta)` | Periodically on the extension's thread. `delta` is actual elapsed ms since last tick. |
| `onExit()` | Once on shutdown. Must complete within 200ms or HPR force-detaches the thread. |

**New Lua APIs in v0.9.3:**

| API | What it does |
|---|---|
| `HPR.dbQueryNumber_E` | Concurrent numeric query across live and historical databases |
| `HPR.dbQueryRange_E` | Same but for ranges of values |
| `HPR.getSystemConfig_E` | Read HPR config values from Lua |
| `HPR.setUiImage_E` | Paint a raw RGBA pixel buffer directly onto a UI panel |

**Documentation:**

| Guide | Link |
|---|---|
| QuickStart + Common Mistakes | [hpr-cpp.netlify.app/quickstart.html](https://hpr-cpp.netlify.app/quickstart.html) |
| Function Overriding API | [hpr-cpp.netlify.app/overrides.html](https://hpr-cpp.netlify.app/overrides.html) |
| Building a Custom Window Backend | [hpr-cpp.netlify.app/custom-app-extension.html](https://hpr-cpp.netlify.app/custom-app-extension.html) |
| EventHub & Extension Communication | [hpr-cpp.netlify.app/comm-bw-extension.html](https://hpr-cpp.netlify.app/comm-bw-extension.html) |
| Custom Themes & UI Hacking | [hpr-cpp.netlify.app/themes.html](https://hpr-cpp.netlify.app/themes.html) |
| Full API Reference | [hpr-cpp.netlify.app/api.html](https://hpr-cpp.netlify.app/api.html) |

---

## System Tray

HPR lives in your system tray and keeps running when you close the window. The only way to quit is through the tray.

**Windows:** Right click for a context menu with **Show HPR** and **Quit**. Closing the window hides to tray.

**Linux (Waybar, KDE, Cinnamon):** HPR registers as a `org.kde.StatusNotifierItem` on the session D-Bus. No GTK. No Qt. Pure `libdbus-1`. Left or right click opens HPR. Middle click quits. Waybar routes both left and right click to the same D-Bus method -- this is a Waybar limitation.

---

## Data Storage

```
~/.local/share/HPR/HPR_DB/          (Linux)
%APPDATA%\HPR\HPR_DB\               (Windows)

    05-26/
        01-05-26.db
        02-05-26.db
        ...
```

One `.db` file per day. One folder per month. Standard SQLite3 that any viewer can open. Delete last month by deleting the folder. No export step. No proprietary format. No account required.

A normal day of use is 30 to 100 KB. A full year sits under 50 MB total.

---

## Installation

**Arch Linux (AUR)**
```bash
yay -S hpr
```

**Windows**

Download and run the setup executable. The Inno Setup installer handles placing `aliases.csv`, `tabAliases.csv`, `config.csv`, and the `ui/` folder into your config directory. It also drops the latest default UI into `ui-REFERENCEONLY/` every update so you always have a clean reference to diff against.

**Linux (Manual)**
```bash
chmod +x installHPRConfigAndUi.sh
./installHPRConfigAndUi.sh
./HPR
```

On first launch HPR automatically creates a desktop entry at `~/.local/share/applications/hpr.desktop`. The entry is only written when missing or stale -- HPR checks whether the `Exec=` and `Icon=` fields already point to the current binary, and only rewrites if they don't.

> [!NOTE]
> If you installed via the AUR, the system-wide desktop entry is already managed by the package. HPR detects this and skips the local entry entirely.

<details>
<summary>Where does everything go?</summary>

**Config**
```
Linux:   ~/.config/HPR/
Windows: %APPDATA%\HPR\HPR_Config\
```

**Data**
```
Linux:   ~/.local/share/HPR/HPR_DB/
Windows: %APPDATA%\HPR\HPR_DB\
```

</details>

---

## Platform Support

| Platform | Backend | Extra Setup |
|---|---|---|
| Hyprland (Wayland) | `hyprctl` IPC | None |
| GNOME (Wayland) | Custom GNOME Shell extension ([lol-another-window-extension](https://github.com/plexescor/lol-another-window-extension)) | One-time only -- see below |
| KDE Plasma 6+ (Wayland / X11) | KWin D-Bus scripting | None |
| Cinnamon (X11 + Wayland) | `org.Cinnamon.Eval` D-Bus method | None |
| niri (Wayland) | `niri msg` IPC | None |
| Windows 10 / 11 | Win32 API | None |

<details>
<summary>GNOME setup walkthrough</summary>

On first launch HPR checks whether its GNOME extension is active. If it is not, it tells you directly. Run the bundled `installWindowCallsExtension.sh`, which installs [lol-another-window-extension](https://github.com/plexescor/lol-another-window-extension) -- a custom shell extension built specifically for HPR.

Because GNOME on Wayland cannot hot-reload shell extensions, you need to log out and back in once after installation. Every subsequent launch is fully automatic from that point on.

If the extension is absent, HPR sets its internal platform identifier to `GNOME_NO_EXTENSION` and returns an instruction string from the poll loop rather than a window name. It will not attempt to run the install script on its own.

</details>

---

## Performance

HPR uses around **8 MB RSS on Windows** and around **27 MB private footprint on Linux** (~47 MB reported by tools like htop due to shared GPU library pages that Mesa maps into every GPU-accelerated process on your system -- not memory HPR owns privately).

To eliminate GPU overhead entirely, set `hardware-acceleration,false` in `config.csv`. This switches Slint to a CPU software renderer. The UI is visually identical and the only cost is slightly higher CPU usage during redraws.

**CPU usage during normal operation: 1 to 3% on modern hardware. Startup time: instant.**

---

## Privacy

```
No accounts.
No telemetry (unless opted-in).
No analytics (unless opted-in).
100% offline core by default.
```

Networking code (`WinHTTP` on Windows, `libcurl` on Linux) is compiled into the HPR binary to power the Lua extension engine, opt-in anonymous analytics, and the developer's YouTube Now Playing display sync. By default, HPR runs entirely offline and never phones home.

**Network controls:**

- **Allow Network Activity**: Disable all outgoing network calls globally under the **Settings** view. When disabled, HPR's HTTP client skips all outgoing GET/POST/PUT requests entirely.

- **YouTube Now Playing**: When enabled on the developer's machine, HPR pushes the currently playing YouTube video title to a Firebase Realtime Database. Normal users' clients read this data to display a "Now Playing" badge on the About page. Normal clients do not upload their own YouTube activity. You can disable this retrieval by turning off network activity in Settings.

- **Anonymous Telemetry (opt-in, off by default)**: If you enable it, HPR generates a random UUIDv4 and reports two anonymous numbers: a one-time registration ping and a weekly active ping (only sent if you used HPR 4 or more days in a single week). No window titles, URLs, project names, or personal logs ever leave your machine. Toggle it on or off at any time under the **Settings** view.

> [!WARNING]
> **Extension Security Warning:**
> Extensions have access to the networking API (`HPR.httpGet_E`) and can read your focus databases and active window titles. A malicious script could read your window history and exfiltrate it. **Only install extensions you fully trust and have reviewed.**

---

## Comparison With Other Trackers

| Feature | HPR | ActivityWatch | RescueTime | Toggl |
|---|---|---|---|---|
| Binary size | ~5 MB | 200 MB+ | Cloud app | Cloud app |
| RAM | ~27 MB private / ~47 MB reported (Linux), ~8 MB (Windows) | 200 MB+ | N/A | N/A |
| Account required | No | No | Yes | Yes |
| Data leaves your machine | Never (unless opted-in) | Never | Yes | Yes |
| Automatic tracking | Yes | Yes | Yes | No |
| Native Wayland | Yes | Partial | N/A | N/A |
| System tray | Yes (native, no libs) | Yes | Yes | Yes |
| Browser tab tracking | Yes (built-in, no extension) | Optional extension | Required extension | Optional extension |
| VS Code project tracking | Yes (built-in, no extension) | Via plugin | No | No |
| Per-app limits & goals | Yes (with force-quit option) | No | Limits only (premium) | No |
| Multi-day historical queries | Yes (Range / Last N Days) | Web dashboard only | No | No |
| Lua extension engine | Yes | No | No | No |
| Open source | Yes | Yes | No | No |
| Launch time | Instant | Several seconds | N/A | N/A |
| Free | Yes | Yes | Limited | Limited |

---

## Aliases

Raw window titles from the OS are inconsistent. `Visual Studio Code` on one machine, `code` on another, `code.exe` on Windows. HPR ships with an `aliases.csv` that collapses all of those into one label. For browser tabs, `tabAliases.csv` handles collapsing page titles into website names.

Adding your own is one line in a CSV: `raw substring,Display Name`. Lines starting with `#` are comments.

> [!TIP]
> Aliases hot-reload. Save the file and HPR picks it up within the next UI tick. No restart.

The raw OS string is always preserved in the database. Aliases apply only at display time -- renaming an alias retroactively updates every historical entry for that application with zero migration work.

---

## Config

`config.csv` is intentionally small:

```csv
use-interpreter,false            # true = load UI from config dir at runtime instead of compiled-in UI
hardware-acceleration,true       # false = CPU renderer, eliminates GPU library overhead on Linux
kill-apps,true                   # false = disable automatic force-quit when daily limits are exceeded
kill-cooldown,2500               # minimum ms between sequential force-quits
poll-interval,50                 # window focus check frequency in ms
db-flush-interval,10000          # how often tracking totals are saved to disk in ms
extension-shutdown-timeout,300   # ms to wait for Lua extensions to finish onExit before force exiting
extension-reload-timeout,450     # ms to wait for Lua extensions to finish on reload/unload before detaching
ui-update-interval,200           # UI update frequency in ms
ui-insight-interval,1000         # interval to run pattern analyzer and update insights in ms
ui-error-duration,5000           # duration for active errors displayed on the UI in ms
allow-network-activity,true      # false = disable all outgoing HTTP calls globally
anonymous-telemetry,false        # true = opt in to anonymous usage analytics
true-headless-mode,false         # true = run as a pure background daemon with no window or graphics context
```

```
Linux:   ~/.config/HPR/config.csv
Windows: %APPDATA%\HPR\HPR_Config\config.csv
```

### True Headless Mode

Setting `true-headless-mode,true` bypasses the Slint UI entirely. No window is created, no graphics context is initialized. The database manager, limits manager, window tracking thread, and all active Lua extensions run normally while the main thread blocks indefinitely. This is a full daemon mode -- useful for running HPR as a background service without any visible presence.

This is distinct from regular headless mode. True headless means zero UI, zero graphics, nothing.

### Single Instance

HPR enforces a single running instance. If you launch HPR while one is already running, the new process sends a `show` signal to the existing instance (which brings its window to focus) and exits immediately. Uses Named Pipes on Windows and Unix Domain Sockets on Linux/macOS.

### Settings View

The **Settings View** in the sidebar exposes various configuration options directly from the UI without needing to edit `config.csv` manually.

### Autostart

HPR can register itself to launch on login. On Windows it writes to the registry autorun key. On Linux it manages a `.desktop` file at `~/.config/autostart/hpr.desktop`. Toggle it in the Settings View.

### Feedback

The **Feedback View** in the sidebar lets you submit bug reports and messages directly from inside HPR. It bundles your OS, compositor/DE, HPR version, and timestamp automatically alongside your email and message.

---

## Custom Themes & Customizing the UI (Advanced)

HPR's UI is built using the [Slint UI Framework](https://docs.slint.dev/latest/docs/slint/). You can load custom themes without modifying any C++ code or recompiling.

### Enabling Themes Mode
```csv
use-interpreter,true
```

### How Themes Work

Themes are loaded from:
- **Windows**: `%APPDATA%\HPR\HPR_Config\themes\`
- **Linux**: `~/.config/HPR/themes\`

Each theme lives in its own subdirectory and must contain:
1. **`metadata.csv`**: Properties like `name,My Theme` and `version,1.0`.
2. **`types.slint`**: Defines data models. Keep this exactly as-is.
3. **`app-window.slint`**: The main layout file, loaded at runtime.
4. **Previews (Optional)**: Up to 9 screenshots (`1.png` through `9.png`) shown in the Themes carousel.

Theme creators have full freedom to build any layout they want and can add or remove any other files as long as the mandatory files are present and satisfy the UI contract with the C++ backend.

For the complete UI contract and required bindings, refer to [hpr-cpp.netlify.app/themes.html](https://hpr-cpp.netlify.app/themes.html).

---

## Roadmap

The foundational work is mostly done: local-first tracking, privacy architecture, Insights engine, native Wayland support, extension engine, offline data ownership. What comes next is refinement -- polish, stability, and quality-of-life improvements.

HPR is completely free. If it is useful to you, consider supporting development on Ko-fi.

---

# For Developers and Power Users

---

## Architecture Overview

HPR is a multi-threaded C++23 application organized around a single shared state struct. Each loaded extension adds its own dedicated thread on top of the baseline HPR threads:

```
Main Thread          (Slint event loop)
  Window Poller      [50ms  tick  -  CurrentWindowManager]
  UI Bridge          [200ms tick  -  HPR / HPRInterpreter + UiModelManager]
  Database Writer    [10s   tick + event-driven  -  DatabaseManager]
  Limits Monitor     [background  -  LimitsManager]
  Extension Threads  [N threads, one per loaded extension  -  ExtensionManager]

Historical Loader: spawns ad-hoc thread on date selection, emits result via EventHub
```

**Main thread** is `main.cpp`. It instantiates `ConfigManager`, `DatabaseManager`, and `CurrentWindowManager`, picks either `HPR` or `HPRInterpreter` based on config, then enters the Slint event loop.

**Window poller** lives in `CurrentWindowManager::getCurrentWindow_Loop`. It calls the platform-specific window getter every 50ms, acquires `stateMutex`, and updates the current window name and accumulated time.

**UI bridge** is `HPR::trackingLoop` or `HPRInterpreter::trackingLoop`. It wakes every 200ms, reads application and tab state, and dispatches model updates to the Slint main thread using `UiModelManager` via `slint::invoke_from_event_loop`.

**Database writer** is `DatabaseManager::writeLoop`. It flushes to SQLite every 10 seconds and also responds to `LOAD_DATABASE_SINGULAR` events to load historical data asynchronously.

---

## Shared State and Synchronization

All mutable shared data lives in one place:

```cpp
namespace AppState {
    struct AppState {
        std::string currentWindow;
        std::string previousWindow;
        std::map<std::string, uint64_t> timeLog_PerApp;
        std::map<std::string, uint64_t> timeLog_PerTab;
        std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> switchHistory;
    };
    extern AppState state;
    extern std::mutex stateMutex;
}
```

Instantiated exactly once in `appState.cpp`. Every thread that touches it acquires `stateMutex` via `std::lock_guard`. The locking strategy is deliberately coarse-grained: lock the whole struct, copy what you need, release immediately, do all work on the copy.

`timeLog_PerApp` and `timeLog_PerTab` accumulate raw millisecond durations. `switchHistory` keys on `std::pair<string, string>` (from, to) and stores a `vector<uint64_t>` of Unix millisecond timestamps for every recorded transition.

**Extension event dispatch** is thread-safe as of v0.9.3. Extension callbacks are no longer fired directly from background threads. Incoming events are queued under `eventQueueMutex` and processed sequentially on the next `onTick` under `luaMutex`, eliminating a class of race conditions and segfaults that could occur with concurrent event dispatch.

---

## Event System

The UI layer and database layer have no direct references to each other. They communicate through `EventHub`, a centralized in-process pub/sub bus with typed payloads.

```cpp
// Subscribe
EventHub::connect(Event::HISTORY_LOADED_SINGULAR, [this](EventData data) { ... });

// Publish
EventHub::emit(Event::LOAD_DATABASE_SINGULAR, DatabaseDate_Singular{requestedDate});

// Cleanup
EventHub::disconnect(Event::HISTORY_LOADED_SINGULAR, id);
```

`EventData` is a `std::variant`. Payloads are type-safe at the call site. Subscribers get an integer ID on connection and use it to unsubscribe in their destructor.

---

## UI Bridging and Slint Interoperability

Two execution modes, one abstraction layer:

| Mode | Class | Mechanism |
|---|---|---|
| Compiled | `HPR` | Slint generates C++ from `.slint` at build time. Maximum performance, smallest footprint. |
| Interpreted | `HPRInterpreter` | Slint loads `.slint` from the config directory at runtime. Modify the UI without rebuilding. |

`UiModelManager` abstracts the difference. All writes are dispatched to the main thread via `slint::invoke_from_event_loop` because Slint UI objects are not thread-safe.

Model sync uses a surgical in-place update rather than clearing and repopulating. Clearing the model causes layout panics during resize and maximize. Property setters also check whether a value has actually changed before calling into Slint (`setPropIfChanged`), eliminating redundant redraws and event loop triggers from no-op updates.

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

`sqlite_modern_cpp` is a header-only C++ wrapper over SQLite3. SQLite3 is the official single-file amalgamation compiled directly into the binary. Zero external database dependencies.

**Write strategy per table:**

```
app_usage      UNIQUE on app name   ->  INSERT OR REPLACE   ->  one row per app, always current
switch_history UNIQUE on timestamp  ->  INSERT OR IGNORE    ->  dump full history every flush, SQLite drops duplicates
```

**Every connection opens with:**
```sql
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
```

A passive WAL checkpoint runs after every write cycle. This was added after hitting real WAL corruption on Btrfs with LUKS encryption during development.

The writer sleeps in 100 intervals of 100ms rather than one 10-second block. HPR exits within 100ms of shutdown instead of hanging for a sleep to expire.

**Single-instance lock:**

| Platform | Mechanism | On crash |
|---|---|---|
| Windows | `CreateFileA` with `FILE_FLAG_DELETE_ON_CLOSE` | Lock file auto-deletes even if HPR crashes hard |
| Linux | `flock(LOCK_EX | LOCK_NB)` | Kernel releases the lock automatically on process death |

---

## Timing Model

| Clock | Role |
|---|---|
| `std::chrono::steady_clock` | Duration measurement between poll ticks. Monotonic. Immune to NTP corrections, DST transitions, and manual clock changes. |
| `std::chrono::system_clock` | Recording switch timestamps for display only. Never used in arithmetic. |

Using `system_clock` for duration measurement is a classic bug that corrupts accumulated totals when NTP fires or DST changes mid-session. Measurement and display use different clocks on purpose.

All millisecond tracking accumulators use `uint64_t` to guarantee no overflow even during extremely long tracking runs.

---

## Pattern Analysis Engine

`PatternAnalyzer` runs behind the Insights view, computing both real-time daily metrics and advanced cross-day trend analyses.

### Real-Time Daily Analysis (Patterns 1-7)

Every 30 seconds inside `trackingLoop`, the engine acquires `stateMutex`, clones the active tracking data, and performs 7 analysis passes on the copy.

- **Patterns 1-5 (Direct Aggregations)**: Scans `timeLog_PerApp` and `switchHistory` to find the most-used application, total tracked time, total switches, and the most switched-away-from/switched-to applications.
- **Pattern 6 (Longest Focus Session)**: Uses a Chronological Event-Matching Algorithm. The raw `switchHistory` map is flattened into a unified event timeline sorted globally by timestamp in O(N log N). One pass pairs each arrival with its next departure. Orphaned arrivals from crashes or force-quits are discarded.
- **Pattern 7 (Peak Productive Hour)**: Uses a Sliding Window Heuristic. A window constrained between 60 and 90 minutes slides across the consolidated timestamp list. The window with the lowest switch frequency is identified as the peak focus block.

### Advanced Cross-Day Analysis

When the user switches to the Insights view, HPR asynchronously queries historical SQLite databases, streaming multiple daily records into a structured `std::vector<DayData>` and passing it to the `PatternAnalyzer`.

**Core algorithms:**

- **Config-Driven App Auto-Detection**: If not explicitly set in `config.csv`, the engine scans all historical data and designates the highest-duration `WORK`-categorized app as the primary work application and the highest-duration `BROWSER`-categorized app as the primary browser.
- **Escape Pattern & Return Rate**: Builds a chronological switch timeline per day, counting transitions from a `WORK` app directly to a `BROWSER` app. Return Rate checks the transition immediately following each escape.
- **Focus Dip Hour**: Builds hourly switch frequency buckets across all loaded days and isolates the hour where multitasking spikes most consistently.
- **Deep Work Before Noon**: Runs the Event-Matching Algorithm on each day's history, extracts the longest single focus session, and calculates the percentage of days where it began before 12:00 PM local time.
- **Weekend vs Weekday Habits**: Splits `DayData` by calendar day-of-week and computes the percentage difference in work-app usage between weekdays and weekends.

---

## Window Name Normalization and Aliasing

`validateAndUpdateWindow_Cross` in `validateAndUpdateWindow.cpp` is the first normalization pass:

```cpp
if (windowName.contains("searchhost")
    || windowName.contains("plasmashell")
    || windowName.contains("js::")       // KWin JS runtime artifact during injection
    || windowName.contains("null")
    // ...
    )
    return "Unknown";
```

The `js::` filter is specifically for KDE. The KDE backend injects a JavaScript payload into KWin via `qdbus6` (or `qdbus-qt6` on Fedora, auto-detected at startup) on every tick. During that injection KWin's own JS runtime briefly appears as the active window. Without this filter, strings like `js::kwin_tmp_1234` silently accumulate time every poll cycle.

`AliasManager` runs an O(N) substring scan through alias rules the first time it sees a new window name, then caches the result in an `unordered_map` for O(1) on every subsequent lookup. The file hot-reloads on change.

---

## Class Lifecycle and Thread Management

Every class that owns a background thread follows the same contract:

```
Constructor  ->  allocate resources, do NOT start the thread
run()        ->  spawn the thread
thread body  ->  check std::atomic<bool> running each iteration
destructor   ->  set running = false, join() if joinable()
```

Shutdown is always clean. The database writer finishes its current flush before the process exits.

---

## Building From Source

**Requirements:**
- CMake 3.21+
- GCC 13+, Clang 16+, or MSVC 2022+ with C++23 support
- Slint 1.16.1 (the install script handles this)
- Linux only: `jq`, `gdbus`, `libdbus-1-dev`, `libcurl4-openssl-dev` (or distro equivalents)
- Windows only: bundled `WinToast` compiles out-of-the-box

```bash
git clone https://github.com/plexescor/HPR
cd HPR
```

**Linux -- install Slint (choose one):**
```bash
# System-wide (requires sudo)
sudo ./installDependencies.sh

# User-local (no sudo)
./installDependencies.sh
```

**Windows:** run `installDependencies.bat`. Pulls Slint 1.16.1, `slint-lsp`, and `slint-viewer` from GitHub releases.

**Build:**
```bash
mkdir build && cd build

# If you ran the install script without sudo:
cmake .. -DCMAKE_PREFIX_PATH="$HOME/.local"

# If you ran with sudo:
cmake ..

cmake --build . --parallel 8
```

HPR uses slightly patched versions of `sol2`, `lua`, and `sqlite3` in `external/` to avoid compile errors under C++23 strict mode. Use the bundled ones for a clean build. CMake copies `aliases.csv`, `config.csv`, `ui/`, `assets/`, and the install scripts next to the output binary automatically -- a fresh build is immediately runnable from the build directory.

---

## Adding a New Platform

[Refer to HPR Docs](https://hpr-cpp.netlify.app/docs.html)

---

## Adding New Tracked Data

[Refer to HPR Docs](https://hpr-cpp.netlify.app/docs.html)

---

## Known Issues and Limitations

> [!WARNING]
> **GNOME without the extension:** If [lol-another-window-extension](https://github.com/plexescor/lol-another-window-extension) is absent, HPR sets its internal platform identifier to `GNOME_NO_EXTENSION` and returns an instruction string from the poll loop rather than a window name. It will not attempt to run the install script autonomously. That is intentional behavior, not a bug.

> [!NOTE]
> **Linux platform detection:** HPR reads `$XDG_CURRENT_DESKTOP` and matches substrings via `std::string::contains`. Non-standard desktop session variables or nested compositor configurations may not resolve correctly.

- Linux install is more manual than Windows (AUR users are covered)
- Waybar routes left and right click to the same D-Bus method -- Waybar limitation, not an HPR one
- Writing invalid `.slint` code in interpreted mode closes HPR -- intentional

---

## Contributing

The full codebase is readable in one sitting if you go in this order:

```
main.cpp              ->  startup, config loading, thread orchestration
appState.hpp          ->  the shared data model, the center of everything
getCurrentWindow.cpp  ->  platform-specific window polling per backend
databaseManager.cpp   ->  persistence, lock file, midnight rollover, historical load
limitsManager.cpp     ->  per-app usage limit and goal monitoring, force-quit, notification dispatch
extensionManager.cpp  ->  Lua VM lifecycle, sol2 bindings, hot-reload, RAII subscription tracking
uiModelManager.cpp    ->  how C++ state becomes Slint models in both UI modes
```

One rule for all new code: anything that touches shared state goes through `AppState::stateMutex`. Lock, copy, release, work on the copy.

No formal process. Open an issue or submit a pull request.

If HPR has been useful to you, a Ko-fi helps keep development going: [ko-fi.com/plexescor](https://ko-fi.com/plexescor)

---

<p align="center">
  <sub>
    Active development &nbsp;|&nbsp; v0.9.3 &nbsp;|&nbsp;
    Hyprland · GNOME · KDE Plasma · Cinnamon · niri · Windows 10/11 &nbsp;|&nbsp;
    C++23 · Slint 1.16.1 · SQLite3 amalgamation · sqlite_modern_cpp · Lua 5.4 · sol2
  </sub>
</p>