# HPR — Human Pattern Recorder

> **This project is archived and no longer maintained as of October 26, 2026.**

---

## Why

I'm Abhijot Singh (Plexescor), the sole developer and maintainer of HPR. I started this project on May 1, 2026, and over the next 6 months I shipped 23 releases, built a Lua extension engine, native Wayland backends for 5 compositors, a cross-platform installer, a theme system, an extension store, and a full documentation site.

And I'm done.

**Here's the reason:**

C++ is eating me alive. Compilation on my 512GB SSD is a nightmare — WSL alone takes up a chunk of it, and a full C++ build with Slint takes forever and leaves me with almost no space. Testing on both Linux and Windows for every release means double the work, double the time, and double the pain. Every release cycle involves compiling, testing, debugging on two OSes, packaging, and shipping — all solo. No CI I'm willing to maintain. No team to share the load.

The operational overhead of maintaining HPR has become genuinely unsustainable for a 16 year old in school who has exactly zero paid help and exactly one donation ($20 from Jesse Kramer, who I will forever be grateful to).

I gave it everything I had. 977 GitHub contributions in one year. 611 downloads across Linux and Windows with zero paid marketing. 17 weekly active users. 5 contributors. 4 external PRs merged. Users from Europe, North America, Asia, and Oceania. Someone built a custom bspwm backend from my docs. Someone fixed Chromium tab tracking. Someone rewrote the activity dashboard. People actually cared about this project and that genuinely means a lot to me.

But I need to stop.

**Archiving HPR gives me:**
- Freedom from release obligations
- Freedom from dual OS testing
- Freedom from C++ compilation hell
- Space on my SSD
- Time and mental energy for things that can actually make me money

My focus now is on earning. I'm moving toward Minecraft modding and other projects with real monetization potential. HPR was never going to make me money — it's fully free, fully open source, and I was never going to change that. But I need money. I'm 16, I have no phone, no income. I need to prove otherwise with something that actually pays.

---

## What Happened

In 6 months HPR went from nothing to:
- 611 organic downloads across Linux and Windows
- 23 releases
- 5 contributors
- Native Wayland backends for Hyprland, GNOME, KDE, Cinnamon, and niri
- A sandboxed Lua 5.4 extension engine with 25+ C++ function intercepts
- HPR Store, HPR Registry, HPR Website, full documentation
- Listings on AlternativeTo, LinuxLinks, ProductHunt
- A PR merged into ActivityWatch's own README
- Featured on forklore.in by FOSS United
- Applied to NLnet, FOSS United, and FUTO grants

Not bad for a solo 16 year old with a laptop and no mentors.

---

## For Users

The last stable release is **v0.9.13**. It works. It will keep working. The code is permanently public under GPL-3.0 — fork it, learn from it, build on it, do whatever you want.

For alternatives, check out [ActivityWatch](https://activitywatch.net/).

---

## Thank You

To Jesse Kramer — you were the first and only person who paid for something I made. That $20 meant more than you know.

To Gh0st1nSh3II, deadrat-in, prendas-adrian, and srtee — you contributed real code to a stranger's project. That's rare and I genuinely appreciate it.

To everyone who downloaded HPR, filed issues, sent PRs, joined the Discord, and told others about it — thank you. You made this feel real.

It was a good run.

— Plexescor