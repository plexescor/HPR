# HPR - Human Pattern Recorder

**Lightweight activity tracker that shows you where your time actually goes.**

Built for people who want to understand their work patterns without sacrificing performance or privacy.

## What It Does

HPR runs silently in the background and tracks:
- Which applications you use and for how long
- How often you switch between tasks
- When you're most productive
- Your actual work patterns vs what you think they are

No manual timers. No cloud sync. No surveillance. Just honest data about yourself.

## Why HPR?

**Performance First**
- ~3.1 MB total size
- <20MB RAM usage
- 1% CPU usage
- Instant startup, no lag

**Privacy First**
- 100% local - data never leaves your machine
- No telemetry, no analytics, no phone-home
- No accounts, no cloud, no third parties
- Open source - audit it yourself

**Simplicity First**
- Install and forget
- Automatic tracking (no manual logging)
- Clean, minimal interface
- Does one thing well

## Current Features (v0.2)

- [x] Real-time application tracking
- [x] Time breakdown per application
- [x] Context switch counting
- [x] Minimal resource usage
- [x] Dark mode UI

## Planned Features

**v1.0 (Coming Soon)**
- [ ] Pattern detection (find your productive hours)
- [ ] Insight generation (understand your habits)
- [ ] Data export (CSV/JSON)
- [ ] Session history
- [ ] Basic statistics dashboard

**v1.5+**
- [ ] Focus mode assistance
- [ ] Customizable tracking rules
- [ ] Advanced pattern analysis
- [ ] Linux support
- [ ] macOS support (maybe)

## Installation

**Requirements:** Windows 10/11 (not linux support atm) 
                  MSVC (though mingw with windows.h support works)
                  Cmake

## Building From Source
```bash
1- git clone https://github.com/yourusername/HPR
2- cd hpr
3- Download latest SDL3 and put it in external/
4- Make sure directory structure is like this:
    external
    |
    |-SDL3
        |
        |-include
            |
            |-SDL3
                |
                |-(all header files)
        |-lib
            |
            |-x64
                |
                |-(SDL3.dll and SDL3.lib)
            |
            |- (same as above for x86 and arm64)
    |
    |-imgui (included)

5- Create a build folder (mkdir build)
6- cd build
7- cmake ..
8- cmake --build . 
    OR for release build,
9- cmake --build . --config Release

```

## How It Works

HPR uses native OS hooks to detect the active window and tracks time automatically.

(No local data storage, yet, to be implemented)

No screenshots. No keystroke logging. Just application names and durations.

## Philosophy

Modern productivity tools are bloated. They eat your RAM, slow down your system, and send your data to the cloud.

HPR is different:
- **Lightweight** - Runs on potato laptops without breaking a sweat
- **Honest** - Shows you the truth about your time usage
- **Respectful** - Your data stays on your machine, always

This is a tool for understanding yourself, not for surveillance or micromanagement.

## Comparison

| Feature | HPR | ActivityWatch | RescueTime | Toggl |
|:------:|:---:|:-------------:|:----------:|:-----:|
| **Size** | 2.9 MB | 200 MB+ | Cloud-based | Cloud-based |
| **RAM Usage** | < 15 MB | 200 MB+ | N/A | N/A |
| **Privacy** | 100% local | Local | Cloud | Cloud |
| **Auto-tracking** | ✓ | ✓ | ✓ | ✗ |
| **Open Source** | ✓ | ✓ | ✗ | ✗ |
| **Free** | ✓ | ✓ | Limited | Limited |


## FAQ

**Q: Does this take screenshots?**  
A: No. Never. HPR only tracks application names and time durations.

**Q: Does my data leave my computer?**  
A: (Though nothing is stored currently but) No. Everything is stored locally. There's no network code at all.

**Q: What's tracked?**  
A: Application window titles (e.g., "Chrome", "VSCode") and time spent. That's it.

**Q: Can I block certain apps from being tracked?**  
A: Not yet, but it's planned for v1.0.

**Q: Does this work on Mac/Linux?**  
A: Currently Windows-only. Linux and Mac support are on the roadmap.

**Q: Is this really free?**  
A: Yes. MIT licensed. Free forever. No premium tiers, no subscriptions.

## Contributing

Found a bug? Have a feature request? Want to contribute code?

- Open an issue on GitHub
- Submit a pull request
- Share your usage patterns (what insights would help you?)

This is early software built by one person. Feedback is gold.

## Roadmap

**Short term (v1.0):**  
Ship a stable release with pattern detection and insights. Polish the core experience.

**Medium term (v1.5):**  
Add focus assistance features and expand platform support.

**Long term (v2.0+):**  
Advanced analytics, integrations, and whatever the community needs most.

## License

MIT License - Use it however you want. See LICENSE file for details.

## Author

Built by a developer frustrated with bloated activity trackers.

If HPR helps you understand yourself better, that's enough.

---

**Status:** Early development (v0.2) - Functional but rough around the edges.  
**Platform:** Windows 10/11  
**Dependencies:** SDL3 (NOT included)
                  ImGui (included)

*Star this repo if you value lightweight, privacy-respecting tools.*