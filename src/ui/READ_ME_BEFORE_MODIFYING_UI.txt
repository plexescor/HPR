================================================
  HPR — READ ME BEFORE MODIFYING THE UI
================================================

You are free to modify anything in these .slint files.
Colors, layouts, fonts, animations, spacing, component
structure, visual hierarchy — all yours to change.

There is exactly ONE rule:

  DO NOT RENAME ANYTHING THAT HPR'S C++ TALKS TO.

That's it. Everything else is fair game.


================================================
  WHAT YOU MUST NOT RENAME
================================================

In app-window.slint, the following names are
hardcoded in HPR's C++ backend. Rename any of
these and HPR will fail to compile or crash at
runtime.

-- Structs (defined in types.slint) --

  TimeLog
    fields: name, duration, duration_i

  TimeLog_Tab
    fields: name, duration, duration_i

  SwitchHistory
    fields: fromWindow, toWindow, maxTimeStamp


-- Properties (HPR writes these every 500ms) --

  trackedTime_S       (int)
  trackedTime_Tab_S   (int)
  windowName_S        (string)
  timePerApp_S        ([TimeLog])
  timePerTab_S        ([TimeLog_Tab])
  switchHistory_S     ([SwitchHistory])
  mostUsedApp_S       (string)
  totalTrackedTime_S  (string)
  totalSwitches_S     (string)
  mostSwitchedFrom_S  (string)
  mostSwitchedTo_S    (string)
  longestFocus_S      (string)
  peakHour_S          (string)


-- Callbacks (HPR wires these at startup) --

  loadHistoricalData_Singular(string)
  loadInsights()
  loadLiveData()
  tabViewClicked()
  siteViewClicked()
  openKofi()


================================================
  WHAT YOU CAN FREELY CHANGE
================================================

Literally everything else. Including but
not limited to:

  - Background colors, gradients, borders
  - Font sizes, weights, letter spacing
  - Component sizes, padding, spacing
  - Animation durations and easing curves
  - Layout structure (HorizontalLayout,
    VerticalLayout, GridLayout, etc.)
  - Adding entirely new visual components
  - Adding new properties for internal UI logic
  - Adding new callbacks for internal UI logic
    (as long as you don't remove the existing ones)
  - The sidebar — expand it, remove it, redesign it
  - The topbar — rearrange it entirely
  - The date picker trigger — move it anywhere
  - The card components, section headers, app rows
  - Any file in ui/ that isn't app-window.slint
    (live-dot.slint, card.slint, etc.) — these
    are purely visual and HPR doesn't reference
    them by name


================================================
  HOW THE DATwindowA FLOWS INTO YOUR UI
================================================

HPR's C++ backend updates your UI every 500ms.
Here is what it sends and what type it is:

  windowName_S
    The currently active window name (string).
    Already alias-translated if you have aliases.csv set up.
    Already normalized (no trailing newlines, no system noise).

  trackedTime_S
    Total milliseconds tracked today across all apps (int).
    Use this to calculate percentages:
      ratio = entry.duration_i / trackedTime_S

  timePerApp_S
    List of TimeLog structs, sorted by duration descending.
    Each entry:
      name        → display name of the app (string)
      duration    → human readable "2h 14m 30s" (string)
      duration_i  → raw milliseconds as int, use for
                    bar widths, sorting, math

  trackedTime_Tab_S
    Total milliseconds tracked today across all tabs (int).

  timePerTab_S
    List of TimeLog_Tab structs, sorted by duration descending.
    Same fields and behavior as timePerApp_S, but for browser tabs.

  switchHistory_S
    List of SwitchHistory structs, sorted by most
    recent transition first.
    Each entry:
      fromWindow    → app switched away from (string)
      toWindow      → app switched to (string)
      maxTimeStamp  → time of most recent occurrence,
                      formatted as "hh:mm:ss am/pm" (string)

  mostUsedApp_S
    The app name with highest total duration (string).

  totalTrackedTime_S
    Formatted string of total time (e.g. "5h 22m") (string).

  totalSwitches_S
    Total number of app switches (string).

  mostSwitchedFrom_S
    App name that was switched away from most often (string).

  mostSwitchedTo_S
    App name that was switched into most often (string).

  longestFocus_S
    Human readable duration of longest session (string).

  peakHour_S
    The hour with most activity (e.g. "02:00 PM") (string).


================================================
  THE DATE PICKER
================================================

The date picker is a standard Slint DatePickerPopup
from std-widgets. When the user accepts a date, it
calls:

  loadHistoricalData_Singular(dateString)

where dateString is formatted as "DD-MM-YY"
(e.g. "08-05-25").

HPR's C++ picks this up, loads that day's SQLite
file asynchronously, and updates the same properties
(timePerApp_S, switchHistory_S, etc.) with the
historical data. Your UI doesn't need to do anything
special — just keep the callback name intact and the
data will appear automatically.

loadLiveData() switches back to today's live data.
Keep this callback name intact too.

-- Date picker nitpick --

The date string passed to loadHistoricalData_Singular
is constructed in Slint with zero-padding for day,
month, and the last two digits of year:

  (date.day < 10 ? "0" : "")   + date.day   + "-" +
  (date.month < 10 ? "0" : "") + date.month + "-" +
  (Math.mod(date.year, 100) < 10 ? "0" : "") +
   Math.mod(date.year, 100)

This must produce exactly "DD-MM-YY" to match the
SQLite filename format HPR uses on disk. If you move
or reimplement the date picker trigger, preserve this
formatting logic exactly or historical loading will
silently fail to find the file.


================================================
  THE INTERPRETED MODE CONSTRAINT
================================================

HPR loads your modified app-window.slint at runtime
via Slint's interpreter. This means:

  - Slint syntax errors will prevent HPR from starting
  - Check the terminal output for compiler diagnostics
    if HPR fails to launch after your changes
  - The Slint LSP (slint-lsp) can catch errors before
    you run HPR — install it for your editor
  - slint-viewer 
    lets you preview .slint files without running HPR


================================================
  REFERENCE UI
================================================

The ui-REFERENCEONLY/ folder next to this file
always contains the latest default UI that shipped
with this version of HPR.

If your modifications break something and you want
to start fresh, copy from ui-REFERENCEONLY/ back
into ui/.

If you updated HPR and something stopped working,
diff your ui/ against ui-REFERENCEONLY/ to see
what changed in the new version.


================================================
  QUICK REFERENCE — SAFE VS UNSAFE CHANGES
================================================

SAFE — go ahead:
  Changing any color or background
  Changing any font property
  Changing padding, spacing, margins
  Changing animation timing
  Restructuring layouts
  Adding new components
  Adding new internal properties
  Renaming components that are only in .slint
  Editing any file other than app-window.slint
  Deleting visual-only components

UNSAFE — will break HPR:
  Renaming TimeLog
  Renaming TimeLog_Tab
  Renaming SwitchHistory
  Renaming any field inside those structs
  Renaming trackedTime_S
  Renaming trackedTime_Tab_S
  Renaming windowName_S
  Renaming timePerApp_S
  Renaming timePerTab_S
  Renaming switchHistory_S
  Renaming mostUsedApp_S
  Renaming totalTrackedTime_S
  Renaming totalSwitches_S
  Renaming mostSwitchedFrom_S
  Renaming mostSwitchedTo_S
  Renaming longestFocus_S
  Renaming peakHour_S
  Renaming loadHistoricalData_Singular
  Renaming loadInsights
  Renaming loadLiveData
  Renaming tabViewClicked
  Renaming siteViewClicked
  Renaming openKofi
  Changing the type of any in property
  Removing any of the above entirely
  Changing the date string format passed to
    loadHistoricalData_Singular (must be "DD-MM-YY")


================================================
  HPR v0.4
  github.com/plexescor/HPR
================================================