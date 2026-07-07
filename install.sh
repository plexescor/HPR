#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Define color codes for pretty output
BOLD='\033[1m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BOLD}=================================================${NC}"
echo -e "${BOLD}         HPR Linux Installer & Updater           ${NC}"
echo -e "${BOLD}=================================================${NC}"
echo ""

# 1. Dependency Verification
echo ">> Checking system dependencies..."
DEPENDENCIES=(curl tar xz dbus-send git)
MISSING_DEPS=()
for cmd in "${DEPENDENCIES[@]}"; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        MISSING_DEPS+=("$cmd")
    fi
done

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo -e "${RED}Error: The following required dependencies are missing:${NC}"
    for dep in "${MISSING_DEPS[@]}"; do
        echo "  - $dep"
    done
    echo "Please install them using your package manager and try again."
    exit 1
fi
echo "   All dependencies satisfied."
echo ""

# 2. Interactive Confirmation
read -p ">> This script will download and install HPR on your system. Proceed? (Y/n): " confirm < /dev/tty
if [[ -n "$confirm" && ! "$confirm" =~ ^[Yy] ]]; then
    echo "Installation cancelled."
    exit 0
fi
echo ""

# 3. Latest Version Fetching
echo ">> Fetching latest release information from GitHub..."
RELEASE_JSON=$(curl -s https://api.github.com/repos/plexescor/HPR/releases/latest)
TAG_NAME=$(echo "$RELEASE_JSON" | grep -oP '"tag_name":\s*"\K[^"]+')

if [ -z "$TAG_NAME" ]; then
    # Fallback to redirect location
    TAG_NAME=$(curl -sL -o /dev/null -w %{url_effective} https://github.com/plexescor/HPR/releases/latest | grep -oP 'tag/\K[^/]+$' || true)
fi

if [ -z "$TAG_NAME" ]; then
    echo -e "${RED}Error: Could not retrieve the latest version from GitHub.${NC}" >&2
    exit 1
fi

VERSION_NUM=$(echo "$TAG_NAME" | sed 's/^v//')
echo "   Latest version: v$VERSION_NUM (tag: $TAG_NAME)"
echo ""

# 4. Download and Extract
TEMP_DIR=$(mktemp -d -t hpr-installer-XXXXXX)
# Ensure cleanup on exit
cleanup() {
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

ASSET_NAME="HPRv${VERSION_NUM}-Linux.tar.xz"
ASSET_URL="https://github.com/plexescor/HPR/releases/download/${TAG_NAME}/${ASSET_NAME}"

echo ">> Downloading $ASSET_NAME..."
if ! curl -fL --progress-bar -o "$TEMP_DIR/$ASSET_NAME" "$ASSET_URL"; then
    echo -e "${RED}Error: Failed to download the release asset from $ASSET_URL${NC}" >&2
    exit 1
fi

echo ">> Extracting archive..."
tar -xf "$TEMP_DIR/$ASSET_NAME" -C "$TEMP_DIR"

# Locate the extracted source directory containing HPR
if [ -f "$TEMP_DIR/HPR" ]; then
    SRC_DIR="$TEMP_DIR"
else
    SRC_DIR=$(find "$TEMP_DIR" -type f -name "HPR" -exec dirname {} \; | head -n 1)
fi

if [ -z "$SRC_DIR" ] || [ ! -d "$SRC_DIR" ]; then
    echo -e "${RED}Error: Could not locate HPR binary in the extracted archive.${NC}" >&2
    exit 1
fi

# 5. System-Wide Binary Installation
# Find existing installation
INSTALL_PATH=""
if [ -f "/usr/local/bin/HPR" ]; then
    INSTALL_PATH="/usr/local/bin/HPR"
elif [ -f "/usr/local/bin/hpr" ]; then
    INSTALL_PATH="/usr/local/bin/hpr"
elif [ -f "/usr/bin/HPR" ]; then
    INSTALL_PATH="/usr/bin/HPR"
elif [ -f "/usr/bin/hpr" ]; then
    INSTALL_PATH="/usr/bin/hpr"
else
    INSTALL_PATH="/usr/local/bin/HPR"
fi

echo ">> System-wide installation path config:"
read -p "   Install location [Press Enter for '$INSTALL_PATH', or type custom path]: " input_path < /dev/tty
if [ -n "$input_path" ]; then
    INSTALL_PATH="$input_path"
fi

INSTALL_DIR=$(dirname "$INSTALL_PATH")
if [ ! -d "$INSTALL_DIR" ]; then
    echo ">> Creating directory $INSTALL_DIR..."
    sudo mkdir -p "$INSTALL_DIR"
fi

echo ">> Installing HPR binary system-wide to $INSTALL_PATH..."
# Unlink first to prevent "Text file busy" error if HPR is currently running
if [ -f "$INSTALL_PATH" ]; then
    sudo rm -f "$INSTALL_PATH"
fi
sudo install -m 755 "$SRC_DIR/HPR" "$INSTALL_PATH"
echo "   Binary installed successfully."

# Install any dynamic libraries (*.so)
echo ">> Installing dynamic libraries (*.so) to $INSTALL_DIR..."
find "$SRC_DIR" -name "*.so*" | while read -r so_file; do
    if [ -e "$so_file" ] || [ -L "$so_file" ]; then
        target_so="$INSTALL_DIR/$(basename "$so_file")"
        if [ -e "$target_so" ] || [ -L "$target_so" ]; then
            sudo rm -f "$target_so"
        fi
        sudo cp -d "$so_file" "$target_so"
        echo "   Copied $(basename "$so_file") to $INSTALL_DIR"
    fi
done
echo ""

# 6. User Configuration Setup
CONFIG_DIR="$HOME/.config/HPR"
DATA_DIR="$HOME/.local/share/HPR"

echo ">> Setting up user configuration directories..."
mkdir -p "$CONFIG_DIR"
mkdir -p "$DATA_DIR"
echo "   Config directory: $CONFIG_DIR"
echo "   Data directory  : $DATA_DIR"
echo ""

# Check config files: aliases.csv, tabAliases.csv, projectAliases.csv, config.csv
CSV_FILES=("aliases.csv" "tabAliases.csv" "projectAliases.csv" "config.csv")
for csv in "${CSV_FILES[@]}"; do
    # Locate CSV file source
    CSV_SRC=""
    if [ -f "$SRC_DIR/$csv" ]; then
        CSV_SRC="$SRC_DIR/$csv"
    elif [ -f "$SRC_DIR/shippedWithBinary/$csv" ]; then
        CSV_SRC="$SRC_DIR/shippedWithBinary/$csv"
    fi

    if [ -n "$CSV_SRC" ]; then
        if [ ! -f "$CONFIG_DIR/$csv" ]; then
            cp "$CSV_SRC" "$CONFIG_DIR/$csv"
            echo "   Copied default config: $csv"
        else
            echo "   Preserved customized config: $csv"
        fi
    fi
done

# UI and assets directories sources
UI_SRC=""
if [ -d "$SRC_DIR/ui" ]; then
    UI_SRC="$SRC_DIR/ui"
elif [ -d "$SRC_DIR/shippedWithBinary/ui" ]; then
    UI_SRC="$SRC_DIR/shippedWithBinary/ui"
fi

ASSETS_SRC=""
if [ -d "$SRC_DIR/assets" ]; then
    ASSETS_SRC="$SRC_DIR/assets"
elif [ -d "$SRC_DIR/shippedWithBinary/assets" ]; then
    ASSETS_SRC="$SRC_DIR/shippedWithBinary/assets"
fi

# Check if active UI was modified by the user before changing REFERENCEONLY
UPDATE_ACTIVE_UI=false
if [ ! -d "$CONFIG_DIR/ui" ]; then
    UPDATE_ACTIVE_UI=true
elif [ -d "$CONFIG_DIR/ui-REFERENCEONLY" ]; then
    if diff -r "$CONFIG_DIR/ui" "$CONFIG_DIR/ui-REFERENCEONLY" >/dev/null 2>&1; then
        UPDATE_ACTIVE_UI=true
    fi
fi

# Always update reference
if [ -n "$UI_SRC" ]; then
    if [ ! -d "$CONFIG_DIR/ui-REFERENCEONLY" ] || ! diff -r "$UI_SRC" "$CONFIG_DIR/ui-REFERENCEONLY" >/dev/null 2>&1; then
        echo ">> Updating ui-REFERENCEONLY to latest..."
        rm -rf "$CONFIG_DIR/ui-REFERENCEONLY"
        cp -r "$UI_SRC" "$CONFIG_DIR/ui-REFERENCEONLY"
    fi
fi

# Update active UI if appropriate
if [ -n "$UI_SRC" ]; then
    if [ "$UPDATE_ACTIVE_UI" = true ]; then
        echo ">> Updating active UI folder to latest..."
        rm -rf "$CONFIG_DIR/ui"
        cp -r "$UI_SRC" "$CONFIG_DIR/ui"
    else
        echo "   Preserved customized ui/ folder. Reference updated at $CONFIG_DIR/ui-REFERENCEONLY/"
    fi
fi

# Update assets
if [ -n "$ASSETS_SRC" ]; then
    if [ ! -d "$CONFIG_DIR/assets" ] || ! diff -r "$ASSETS_SRC" "$CONFIG_DIR/assets" >/dev/null 2>&1; then
        echo ">> Updating assets to latest..."
        rm -rf "$CONFIG_DIR/assets"
        cp -r "$ASSETS_SRC" "$CONFIG_DIR/assets"
    fi
fi
echo ""

# 7. Install Confirmation Message
echo -e "${BOLD}${GREEN}HPR Installed Successfully!${NC}"
echo ""

# 8. GNOME Extension Detection & Direct Installation
if [[ "$XDG_CURRENT_DESKTOP" == *"GNOME"* ]]; then
    EXT_ID="lol-another-window-extension@plexescor"
    EXT_DIR="$HOME/.local/share/gnome-shell/extensions/$EXT_ID"
    
    # Check if installed
    IS_INSTALLED=false
    if [ -d "$EXT_DIR" ] && gnome-extensions list 2>/dev/null | grep -qF "$EXT_ID"; then
        IS_INSTALLED=true
    fi
    
    if [ "$IS_INSTALLED" = false ]; then
        echo -e "${YELLOW}>> GNOME Desktop detected, but HPR's window-tracking extension is not installed/active.${NC}"
        read -p ">> Would you like to install the required GNOME Shell extension? (Y/n): " install_ext < /dev/tty
        if [[ -z "$install_ext" || "$install_ext" =~ ^[Yy] ]]; then
            echo ">> Installing extension..."
            if [ -d "$EXT_DIR" ]; then
                rm -rf "$EXT_DIR"
            fi
            
            # Clone extension directly
            if git clone "https://github.com/plexescor/lol-another-window-extension" "$EXT_DIR"; then
                echo "   Extension downloaded successfully."
                
                # Check if GNOME Shell recognizes the extension
                if gnome-extensions list 2>/dev/null | grep -qF "$EXT_ID"; then
                    echo ">> Enabling GNOME extension..."
                    if gnome-extensions enable "$EXT_ID" >/dev/null 2>&1; then
                        echo -e "${GREEN}   Extension enabled successfully! No restart needed.${NC}"
                    else
                        echo -e "${YELLOW}   Failed to enable the extension automatically. You can enable it via the Extensions app.${NC}"
                    fi
                else
                    echo ""
                    echo -e "${BOLD}=================================================${NC}"
                    echo -e "${BOLD}${YELLOW}   ACTION REQUIRED — GNOME Session Logout Needed${NC}"
                    echo -e "${BOLD}=================================================${NC}"
                    echo "   On GNOME with Wayland, GNOME Shell cannot hot-reload or"
                    echo "   scan for newly copied extensions while running."
                    echo ""
                    echo "   To complete the installation, please:"
                    echo "     1. Save any open work"
                    echo "     2. Log out of your GNOME session"
                    echo "     3. Log back in"
                    echo "     4. HPR will automatically enable the extension and work."
                    echo -e "${BOLD}=================================================${NC}"
                    echo ""
                fi
            else
                echo -e "${RED}Error: Failed to clone the extension repository.${NC}"
            fi
        fi
    else
        echo "   GNOME Shell extension is already installed."
    fi
    echo ""
fi

# 9. Desktop Launcher and Icons
echo ">> Setting up desktop launcher and icons..."
ICON_DIR="$HOME/.local/share/icons/hicolor/256x256/apps"
mkdir -p "$ICON_DIR"
if [ -f "$ASSETS_SRC/logo_256png.png" ]; then
    cp "$ASSETS_SRC/logo_256png.png" "$ICON_DIR/hpr.png"
    echo "   App icon installed."
fi

DESKTOP_DIR="$HOME/.local/share/applications"
mkdir -p "$DESKTOP_DIR"
cat <<EOF > "$DESKTOP_DIR/hpr.desktop"
[Desktop Entry]
Version=1.0
Type=Application
Name=HPR
Comment=Offline zero-account activity tracker
Exec=$INSTALL_PATH
Icon=hpr
Terminal=false
Categories=Utility;
StartupNotify=true
EOF
chmod +x "$DESKTOP_DIR/hpr.desktop"
echo "   Desktop file created."

# Refresh caches if possible
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$DESKTOP_DIR" >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" >/dev/null 2>&1 || true
fi
echo "   Desktop database and icon caches updated."
echo ""
echo -e "${GREEN}HPR installation/update process is complete!${NC}"
