# Contributing to HPR
> HPR is a native C++23 activity tracker for Linux and Windows. If you haven't used it yet, try it before contributing — understanding what it does makes contributing way easier.

Hey! Thanks for wanting to contribute to HPR. Every bit helps — whether you're fixing a typo, hunting down a bug, improving the UI, or building something cool. Genuinely appreciated.

Take a moment to read this before you start so we're on the same page.

## Code of Conduct

HPR has a [Code of Conduct](CODE_OF_CONDUCT.md). Read it. Abide by it.

To report bad behaviour, contact me at saraswal2010@gmail.com or plexescor69@gmail.com.

## What's Accepted

- Bug fixes and code improvements
- UI and design improvements
- Documentation fixes
- Bug reports

## What's Not Accepted

- Translations — not at this stage
- PRs that increase binary size significantly
- PRs that add new dependencies — if your change needs a new library, open an issue and discuss it first, because the answer is probably no
- Large features — charts, dashboards, new tracking backends, big rewrites, anything that significantly expands scope. If it's a big idea, it probably belongs as an HPR extension, not in the core binary. Build it as a Lua extension instead — that's literally what the extension system exists for

## Reporting Bugs

Open an issue. No template, just describe what happened, what you expected to happen, and your OS and compositor. The more detail the better.

## Making Changes

**Branch:** I work off main. Branch off main, PR into main.

**Commit messages:** No enforced format but make them sensible and descriptive. "fix thing" is not acceptable. "fix window title not updating on GNOME after idle timeout" is.

**Code style:** I don't use a specific formatter myself. Just make it look consistent with the surrounding code. If your diff is 80% reformatting and 20% actual change, something went wrong.

**PR description:** No template. Just explain what you changed, why you changed it, and any relevant context. If it fixes a bug, link the issue.

**Open an issue first?** Up to you. For small fixes, just send the PR. For anything non-trivial, opening an issue first to discuss the approach is a good idea so you don't waste time on something that won't be merged.

## Extensions

If you want to build an HPR extension, the extension API docs are in the main repo. Extensions are the right place for anything that would bloat the core binary.

To publish your extension to HPR Store, refer to [HPR-Registry](https://github.com/plexescor/HPR-Registry) and follow the instructions in its `PUBLISHING.md`.

## Questions

Open an issue and ask. I read everything.