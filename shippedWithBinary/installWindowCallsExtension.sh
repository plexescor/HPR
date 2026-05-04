#!/usr/bin/env bash

# =============================================================================
# HPR - GNOME Extension Installer
# Installs: window-calls-extended@hseliger.eu
# =============================================================================

echo "[HPR] Extension not working, attempting install..."

# --- Resolve home directory ---
HOME_DIR="$HOME"
echo "[HPR] Home dir: $HOME_DIR"

EXT_DIR="$HOME_DIR/.local/share/gnome-shell/extensions/window-calls-extended@hseliger.eu"
echo "[HPR] Extension dir: $EXT_DIR"

# --- Clone only if folder doesn't already exist ---
if [ -d "$EXT_DIR" ]; then
    echo "[HPR] Clone result: Directory already exists, skipping clone."
else
    echo "[HPR] Clone result: Cloning repository..."
    CLONE_OUT=$(git clone https://github.com/hseliger/window-calls-extended "$EXT_DIR" 2>&1)
    CLONE_EXIT=$?
    echo "[HPR] Clone result: $CLONE_OUT"
    if [ $CLONE_EXIT -ne 0 ]; then
        echo "[HPR] Clone result: ERROR — git clone failed (exit code $CLONE_EXIT)"
        exit 1
    fi
fi

# --- Enable the extension ---
echo "[HPR] Enabling extension..."
ENABLE_OUT=$(gnome-extensions enable window-calls-extended@hseliger.eu 2>&1)
ENABLE_EXIT=$?
echo "[HPR] Enable result: $ENABLE_OUT"

if [ $ENABLE_EXIT -ne 0 ]; then
    echo "[HPR] Enable result: WARNING — gnome-extensions enable failed (exit code $ENABLE_EXIT)"
fi

# --- Wayland: cannot restart GNOME Shell without logout ---
echo "[HPR] Platform set to GNOME_NEEDS_RESTART"
echo ""
echo "[HPR] ============================================================"
echo "[HPR]  ACTION REQUIRED: Please log out and back in to activate the"
echo "[HPR]  extension. GNOME Shell cannot be restarted on Wayland."
echo "[HPR] ============================================================"
