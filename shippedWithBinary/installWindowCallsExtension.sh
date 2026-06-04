#!/usr/bin/env bash

# =============================================================================
# HPR - GNOME Extension Installer
# Installs: lol-another-window-extension@plexescor
# =============================================================================

EXT_ID="lol-another-window-extension@plexescor"
EXT_DIR="$HOME/.local/share/gnome-shell/extensions/$EXT_ID"
REPO_URL="https://github.com/plexescor/lol-another-window-extension"

echo ""
echo "[HPR] ============================================================"
echo "[HPR]  HPR Extension Installer"
echo "[HPR] ============================================================"
echo ""

# --- Step 1: Check if extension files already exist ---
if [ -d "$EXT_DIR" ]; then
    echo "[HPR] Step 1/3: Extension already exists. Deleting it for a fresh install..."
    rm -rf "$EXT_DIR"
fi

echo "[HPR] Step 1/3: Downloading extension files..."
echo "[HPR]           (This requires git and an internet connection)"
echo ""

if ! command -v git &>/dev/null; then
    echo "[HPR] ERROR: 'git' is not installed on your system."
    echo "[HPR]        Please install it first:"
    echo "[HPR]          Ubuntu/Debian:  sudo apt install git"
    echo "[HPR]          Fedora:         sudo dnf install git"
    echo "[HPR]          Arch:           sudo pacman -S git"
    echo ""
    exit 1
fi

CLONE_OUT=$(git clone "$REPO_URL" "$EXT_DIR" 2>&1)
CLONE_EXIT=$?

if [ $CLONE_EXIT -ne 0 ]; then
    echo "[HPR] ERROR: Download failed. Details below:"
    echo "$CLONE_OUT"
    echo ""
    echo "[HPR]        Common causes:"
    echo "[HPR]          - No internet connection"
    echo "[HPR]          - GitHub is unreachable"
    echo "[HPR]          - Disk is full"
    echo ""
    exit 1
fi

echo "[HPR]           Download complete!"

# --- Step 2: Try to enable the extension ---
# GNOME Shell only knows about extensions that existed at login time.
# If we just downloaded it right now, GNOME Shell won't see it yet —
# we need a logout/login first. So we check if GNOME already knows
# about this extension before trying to enable it.

echo ""
echo "[HPR] Step 2/3: Checking if GNOME Shell recognizes the extension..."

KNOWN=$(gnome-extensions list 2>/dev/null | grep -F "$EXT_ID")

if [ -n "$KNOWN" ]; then
    echo "[HPR]           GNOME Shell recognizes the extension. Enabling..."

    ENABLE_OUT=$(gnome-extensions enable "$EXT_ID" 2>&1)
    ENABLE_EXIT=$?

    if [ $ENABLE_EXIT -eq 0 ]; then
        echo "[HPR]           Extension enabled successfully!"
        echo ""
        echo "[HPR] Step 3/3: All done! You can now launch HPR normally."
        echo ""
        echo "[HPR] ============================================================"
        echo "[HPR]  SUCCESS: Extension is active. No restart needed."
        echo "[HPR] ============================================================"
        echo ""
        exit 0
    else
        echo "[HPR]           WARNING: Enable command failed. Details:"
        echo "              $ENABLE_OUT"
        echo "[HPR]          Will fall through to restart instructions."
    fi
else
    echo "[HPR]           GNOME Shell does NOT recognize the extension yet."
    echo "[HPR]           This is normal after a fresh download —"
    echo "[HPR]           GNOME only scans for new extensions at login time."
fi

# --- Step 3: Restart required ---
echo ""
echo "[HPR] Step 3/3: A logout is required to finish setup."
echo ""
echo "[HPR] ============================================================"
echo "[HPR]  ACTION REQUIRED — Please do the following:"
echo ""
echo "[HPR]    1. Save any open work"
echo "[HPR]    2. Log out of your GNOME session"
echo "[HPR]       (Top-right corner → Power icon → Log Out)"
echo "[HPR]    3. Log back in"
echo "[HPR]    4. Open a terminal and run HPR again"
echo ""
echo "[HPR]  WHY? On Wayland, GNOME Shell cannot reload extensions"
echo "[HPR]  while it's running. A logout lets it restart cleanly"
echo "[HPR]  and pick up the newly installed extension."
echo ""
echo "[HPR]  You only need to do this ONCE. After that, HPR will"
echo "[HPR]  launch normally without any extra steps."
echo ""
echo "[HPR]  NOTE: After logging back in, simply launch HPR as normal."
echo "[HPR]        HPR will automatically finish the setup for you."
echo "[HPR]        You do NOT need to run this script again."
echo "[HPR] ============================================================"
echo ""