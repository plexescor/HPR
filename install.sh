#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Define color codes for pretty output
BOLD='\033[1m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Metadata paths
METADATA_FILE="/etc/HPR/install_metadata.txt"
FALLBACK_METADATA_FILE="$HOME/.local/share/HPR_Installer/install_metadata.txt"

# Temporary directories
TEMP_DIR=""

cleanup_temp() {
    if [ -n "$TEMP_DIR" ] && [ -d "$TEMP_DIR" ]; then
        echo ">> Cleaning up temporary download files..."
        rm -rf "$TEMP_DIR"
        TEMP_DIR=""
        echo "   Cleanup complete."
    fi
}
trap cleanup_temp EXIT

check_dependencies() {
    echo ">> Checking system dependencies..."
    local DEPENDENCIES=(curl tar xz dbus-send git)
    local MISSING_DEPS=()
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
}

fetch_latest_version() {
    echo ">> Fetching latest release information from GitHub..."
    local RELEASE_JSON
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
}

download_and_extract() {
    cleanup_temp
    TEMP_DIR=$(mktemp -d -t hpr-installer-XXXXXX)
    
    local ASSET_NAME="HPRv${VERSION_NUM}-Linux.tar.xz"
    local ASSET_URL="https://github.com/plexescor/HPR/releases/download/${TAG_NAME}/${ASSET_NAME}"
    
    echo ">> Downloading release archive from: $ASSET_URL"
    if ! curl -fL --progress-bar -o "$TEMP_DIR/$ASSET_NAME" "$ASSET_URL"; then
        echo -e "${RED}Error: Failed to download the release asset from $ASSET_URL${NC}" >&2
        exit 1
    fi
    
    echo ">> Extracting release archive..."
    tar -xf "$TEMP_DIR/$ASSET_NAME" -C "$TEMP_DIR"
    echo "   Extraction complete."
    
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
}

write_metadata_file() {
    local target_path="$1"
    echo ">> Persisting installation path metadata..."
    
    # Attempt to write to primary system path
    if sudo mkdir -p "$(dirname "$METADATA_FILE")" >/dev/null 2>&1; then
        if echo "$target_path" | sudo tee "$METADATA_FILE" >/dev/null; then
            echo "   Saved path to primary location: $METADATA_FILE"
            return 0
        fi
    fi
    
    # Fallback path if sudo is unavailable or fails
    mkdir -p "$(dirname "$FALLBACK_METADATA_FILE")"
    echo "$target_path" > "$FALLBACK_METADATA_FILE"
    echo "   Saved path to fallback location: $FALLBACK_METADATA_FILE"
}

remove_metadata_files() {
    echo ">> Removing metadata files..."
    if [ -f "$METADATA_FILE" ]; then
        sudo rm -f "$METADATA_FILE"
        echo "   Removed primary metadata file: $METADATA_FILE"
    fi
    if [ -f "$FALLBACK_METADATA_FILE" ]; then
        rm -f "$FALLBACK_METADATA_FILE"
        echo "   Removed fallback metadata file: $FALLBACK_METADATA_FILE"
    fi
}

get_hpr_path() {
    local PATH_FOUND=""
    
    # 1. Check primary metadata file
    if [ -f "$METADATA_FILE" ]; then
        PATH_FOUND=$(cat "$METADATA_FILE" 2>/dev/null || true)
    fi
    
    # 2. Check fallback metadata file if primary not found
    if [ -z "$PATH_FOUND" ] && [ -f "$FALLBACK_METADATA_FILE" ]; then
        PATH_FOUND=$(cat "$FALLBACK_METADATA_FILE" 2>/dev/null || true)
    fi
    
    # Verify the path actually exists
    if [ -n "$PATH_FOUND" ] && [ -f "$PATH_FOUND" ]; then
        echo "$PATH_FOUND"
        return 0
    fi
    
    # If not found, ask user
    echo -e "${YELLOW}>> HPR installation not detected in default metadata folders.${NC}"
    while true; do
        echo -e ">> If you copied HPR somewhere else, enter the path to HPR binary"
        echo -e "   (include the binary itself in the path, e.g. /home/username/HPR_DIR/HPR)"
        read -p ">> If HPR is truly not installed, press [Enter]: " input_path < /dev/tty
        
        if [ -z "$input_path" ]; then
            echo "   HPR is not installed."
            echo ""
            return 1
        fi
        
        # Check for tilde
        if [[ "$input_path" == *~* ]]; then
            echo -e "${RED}Error: Tilde (~) is not allowed. Please use absolute paths (e.g. /home/username/path).${NC}" >&2
            echo ""
            continue
        fi
        
        # Clean path: strip trailing slashes to clean it up
        cleaned_path="$input_path"
        while [[ "$cleaned_path" == */ && "$cleaned_path" != "/" ]]; do
            cleaned_path="${cleaned_path%/}"
        done
        
        # Double confirm
        read -p ">> Are you sure HPR is located at '$cleaned_path'? (y/N): " confirm_loc < /dev/tty
        if [[ "$confirm_loc" =~ ^[Yy] ]]; then
            echo "$cleaned_path"
            return 0
        else
            echo "   Let's try again."
            echo ""
        fi
    done
}

setup_configs() {
    local CONFIG_DIR="$HOME/.config/HPR"
    local DATA_DIR="$HOME/.local/share/HPR"
    
    echo ">> Setting up user configuration directories..."
    mkdir -p "$CONFIG_DIR"
    mkdir -p "$DATA_DIR"
    echo "   Config directory: $CONFIG_DIR"
    echo "   Data directory  : $DATA_DIR"
    
    # Check config files: aliases.csv, tabAliases.csv, projectAliases.csv, config.csv
    local CSV_FILES=("aliases.csv" "tabAliases.csv" "projectAliases.csv" "config.csv")
    for csv in "${CSV_FILES[@]}"; do
        local CSV_SRC=""
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
    local UI_SRC=""
    if [ -d "$SRC_DIR/ui" ]; then
        UI_SRC="$SRC_DIR/ui"
    elif [ -d "$SRC_DIR/shippedWithBinary/ui" ]; then
        UI_SRC="$SRC_DIR/shippedWithBinary/ui"
    fi
    
    local ASSETS_SRC=""
    if [ -d "$SRC_DIR/assets" ]; then
        ASSETS_SRC="$SRC_DIR/assets"
    elif [ -d "$SRC_DIR/shippedWithBinary/assets" ]; then
        ASSETS_SRC="$SRC_DIR/shippedWithBinary/assets"
    fi
    
    # Check if active UI was modified by the user before changing REFERENCEONLY
    local UPDATE_ACTIVE_UI=false
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
}

setup_gnome_extension() {
    if [[ "$XDG_CURRENT_DESKTOP" == *"GNOME"* ]]; then
        local EXT_ID="lol-another-window-extension@plexescor"
        local EXT_DIR="$HOME/.local/share/gnome-shell/extensions/$EXT_ID"
        
        # Check if installed
        local IS_INSTALLED=false
        if [ -d "$EXT_DIR" ] && gnome-extensions list 2>/dev/null | grep -qF "$EXT_ID"; then
            IS_INSTALLED=true
        fi
        
        if [ "$IS_INSTALLED" = false ]; then
            echo -e "${YELLOW}>> GNOME Desktop detected, but HPR's window-tracking extension is not installed/active.${NC}"
            read -p ">> Would you like to install the required GNOME Shell extension? (Y/n): " install_ext < /dev/tty
            if [[ -z "$install_ext" || "$install_ext" =~ ^[Yy] ]]; then
                echo ">> Installing GNOME extension..."
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
}

setup_desktop_launcher() {
    local binary_path="$1"
    echo ">> Setting up desktop launcher and icons..."
    
    local ASSETS_SRC=""
    if [ -d "$SRC_DIR/assets" ]; then
        ASSETS_SRC="$SRC_DIR/assets"
    elif [ -d "$SRC_DIR/shippedWithBinary/assets" ]; then
        ASSETS_SRC="$SRC_DIR/shippedWithBinary/assets"
    fi

    local ICON_DIR="$HOME/.local/share/icons/hicolor/256x256/apps"
    mkdir -p "$ICON_DIR"
    if [ -n "$ASSETS_SRC" ] && [ -f "$ASSETS_SRC/logo_256png.png" ]; then
        cp "$ASSETS_SRC/logo_256png.png" "$ICON_DIR/hpr.png"
        echo "   App icon installed at $ICON_DIR/hpr.png"
    fi
    
    local DESKTOP_DIR="$HOME/.local/share/applications"
    mkdir -p "$DESKTOP_DIR"
    cat <<EOF > "$DESKTOP_DIR/hpr.desktop"
[Desktop Entry]
Version=1.0
Type=Application
Name=HPR
Comment=Offline zero-account activity tracker
Exec=$binary_path
Icon=hpr
Terminal=false
Categories=Utility;
StartupNotify=true
EOF
    chmod +x "$DESKTOP_DIR/hpr.desktop"
    echo "   Desktop file created at $DESKTOP_DIR/hpr.desktop"
    
    # Refresh caches
    refresh_desktop_caches
}

remove_desktop_launcher() {
    echo ">> Cleaning up desktop launcher and icon files..."
    local DESKTOP_FILE="$HOME/.local/share/applications/hpr.desktop"
    local ICON_FILE="$HOME/.local/share/icons/hicolor/256x256/apps/hpr.png"
    
    if [ -f "$DESKTOP_FILE" ]; then
        rm -f "$DESKTOP_FILE"
        echo "   Deleted desktop launcher: $DESKTOP_FILE"
    fi
    
    if [ -f "$ICON_FILE" ]; then
        rm -f "$ICON_FILE"
        echo "   Deleted app icon: $ICON_FILE"
    fi
    
    # Refresh caches
    refresh_desktop_caches
}

refresh_desktop_caches() {
    echo ">> Refreshing desktop and icon caches..."
    local DESKTOP_DIR="$HOME/.local/share/applications"
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$DESKTOP_DIR" >/dev/null 2>&1 || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" >/dev/null 2>&1 || true
    fi
    echo "   Caches refreshed."
}

install_hpr() {
    echo ">> Initiating HPR Installation process..."
    
    # 1. Dependency Verification
    check_dependencies
    
    # 2. Latest Version Fetching
    fetch_latest_version
    
    # 3. Download and Extract
    download_and_extract
    
    # 4. System-Wide Binary Installation
    local INSTALL_PATH=""
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
    
    echo ">> Configuring installation directory..."
    while true; do
        read -p "   Install location [Press Enter for '$INSTALL_PATH', or type custom path]: " input_path < /dev/tty
        if [ -z "$input_path" ]; then
            break
        fi
        if [[ "$input_path" == *~* ]]; then
            echo -e "${RED}Error: Tilde (~) is not allowed. Please use absolute paths (e.g. /home/username/path).${NC}" >&2
            continue
        fi
        
        # Process custom path: strip trailing slashes to clean it up
        local cleaned_path="$input_path"
        while [[ "$cleaned_path" == */ && "$cleaned_path" != "/" ]]; do
            cleaned_path="${cleaned_path%/}"
        done
        
        local base_name
        base_name=$(basename "$cleaned_path")
        # If the entered path is a directory or has a basename other than HPR/hpr, append /HPR
        if [[ "$input_path" == */ ]] || [ -d "$cleaned_path" ] || [[ "$base_name" != "HPR" && "$base_name" != "hpr" ]]; then
            INSTALL_PATH="$cleaned_path/HPR"
        else
            INSTALL_PATH="$cleaned_path"
        fi
        break
    done
    
    local INSTALL_DIR
    INSTALL_DIR=$(dirname "$INSTALL_PATH")
    if [ ! -d "$INSTALL_DIR" ]; then
        echo ">> Creating target directory: $INSTALL_DIR..."
        sudo mkdir -p "$INSTALL_DIR"
    fi
    
    echo ">> Installing HPR binary to $INSTALL_PATH..."
    if [ -f "$INSTALL_PATH" ]; then
        sudo rm -f "$INSTALL_PATH"
    fi
    sudo install -m 755 "$SRC_DIR/HPR" "$INSTALL_PATH"
    echo "   Binary installed successfully."
    
    # Install any dynamic libraries (*.so)
    echo ">> Installing dynamic libraries to $INSTALL_DIR..."
    find "$SRC_DIR" -name "*.so*" | while read -r so_file; do
        if [ -e "$so_file" ] || [ -L "$so_file" ]; then
            local target_so="$INSTALL_DIR/$(basename "$so_file")"
            if [ -e "$target_so" ] || [ -L "$target_so" ]; then
                sudo rm -f "$target_so"
            fi
            sudo cp -d "$so_file" "$target_so"
            echo "   Copied $(basename "$so_file") to $INSTALL_DIR"
        fi
    done
    echo ""
    
    # Save path metadata
    write_metadata_file "$INSTALL_PATH"
    
    # 5. User Configuration Setup
    setup_configs
    
    # 6. GNOME Extension Setup
    setup_gnome_extension
    
    # 7. Desktop Launcher Setup
    setup_desktop_launcher "$INSTALL_PATH"
    
    cleanup_temp
    echo -e "${BOLD}${GREEN}HPR Installation Complete!${NC}"
    echo ""
}

update_hpr() {
    local current_bin_path
    current_bin_path=$(get_hpr_path) || return 0 # Return to menu if HPR not found/not installed
    
    echo ">> Initiating HPR Update process..."
    
    # 1. Check dependencies
    check_dependencies
    
    # 2. Fetch latest version
    fetch_latest_version
    
    # 3. Download and extract
    download_and_extract
    
    # Determine directory containing HPR
    local INSTALL_PATH="$current_bin_path"
    local INSTALL_DIR
    INSTALL_DIR=$(dirname "$INSTALL_PATH")
    
    echo ">> Cleaning target directory..."
    # safety check for critical system directory
    if [[ "$INSTALL_DIR" == "/usr/local/bin" || "$INSTALL_DIR" == "/usr/bin" || "$INSTALL_DIR" == "/bin" || "$INSTALL_DIR" == "/usr/local" || "$INSTALL_DIR" == "$HOME" || "$INSTALL_DIR" == "/" ]]; then
        echo "   Safety Check: '$INSTALL_DIR' is a critical system path. Directory wiping is disabled."
        echo "   Deleting old binary 'HPR' and dynamic library 'libslint_cpp.so'..."
        sudo rm -f "$INSTALL_PATH"
        sudo rm -f "$INSTALL_DIR"/libslint_cpp.so*
    else
        echo -e "${RED}WARNING: All files inside directory '$INSTALL_DIR' will be deleted!${NC}"
        read -p "Are you absolutely sure you want to delete everything inside '$INSTALL_DIR'? (y/N): " confirm_wipe < /dev/tty
        if [[ "$confirm_wipe" =~ ^[Yy] ]]; then
            echo "   Wiping directory: $INSTALL_DIR..."
            sudo rm -rf "$INSTALL_DIR"/*
        else
            echo "   Wipe cancelled. Deleting only 'HPR' and 'libslint_cpp.so'..."
            sudo rm -f "$INSTALL_PATH"
            sudo rm -f "$INSTALL_DIR"/libslint_cpp.so*
        fi
    fi
    
    echo ">> Installing HPR binary to $INSTALL_PATH..."
    sudo install -m 755 "$SRC_DIR/HPR" "$INSTALL_PATH"
    
    echo ">> Installing dynamic libraries to $INSTALL_DIR..."
    find "$SRC_DIR" -name "*.so*" | while read -r so_file; do
        if [ -e "$so_file" ] || [ -L "$so_file" ]; then
            local target_so="$INSTALL_DIR/$(basename "$so_file")"
            if [ -e "$target_so" ] || [ -L "$target_so" ]; then
                sudo rm -f "$target_so"
            fi
            sudo cp -d "$so_file" "$target_so"
            echo "   Copied $(basename "$so_file") to $INSTALL_DIR"
        fi
    done
    
    # Update configs
    setup_configs
    
    # Save metadata path in case it was custom updated
    write_metadata_file "$INSTALL_PATH"
    
    # Setup desktop files
    setup_desktop_launcher "$INSTALL_PATH"
    
    cleanup_temp
    echo -e "${BOLD}${GREEN}HPR Update Complete!${NC}"
    echo ""
}

remove_hpr() {
    local current_bin_path
    current_bin_path=$(get_hpr_path) || return 0 # Return to menu if HPR not found/not installed
    
    echo ">> Initiating HPR Removal process..."
    
    local INSTALL_PATH="$current_bin_path"
    local INSTALL_DIR
    INSTALL_DIR=$(dirname "$INSTALL_PATH")
    
    echo ">> Deleting files..."
    if [[ "$INSTALL_DIR" == "/usr/local/bin" || "$INSTALL_DIR" == "/usr/bin" || "$INSTALL_DIR" == "/bin" || "$INSTALL_DIR" == "/usr/local" || "$INSTALL_DIR" == "$HOME" || "$INSTALL_DIR" == "/" ]]; then
        echo "   Safety Check: '$INSTALL_DIR' is a critical system path. Directory wiping is disabled."
        echo "   Deleting HPR binary and dynamic library 'libslint_cpp.so'..."
        sudo rm -f "$INSTALL_PATH"
        sudo rm -f "$INSTALL_DIR"/libslint_cpp.so*
    else
        echo -e "${RED}WARNING: All files inside directory '$INSTALL_DIR' will be deleted!${NC}"
        read -p "Are you absolutely sure you want to delete everything inside '$INSTALL_DIR'? (y/N): " confirm_wipe < /dev/tty
        if [[ "$confirm_wipe" =~ ^[Yy] ]]; then
            echo "   Wiping and removing directory: $INSTALL_DIR..."
            sudo rm -rf "$INSTALL_DIR"
        else
            echo "   Wipe cancelled. Deleting only HPR binary and dynamic library 'libslint_cpp.so'..."
            sudo rm -f "$INSTALL_PATH"
            sudo rm -f "$INSTALL_DIR"/libslint_cpp.so*
        fi
    fi
    
    # Remove desktop launcher files and update caches
    remove_desktop_launcher
    
    # Remove metadata
    remove_metadata_files
    
    echo -e "${BOLD}${GREEN}HPR Removal Complete!${NC}"
    echo ""
}

# Interactive Menu Loop
while true; do
    echo -e "${BOLD}=================================================${NC}"
    echo -e "${BOLD}         HPR Linux Installation Manager          ${NC}"
    echo -e "${BOLD}=================================================${NC}"
    echo "  1) Install HPR"
    echo "  2) Update HPR"
    echo "  3) Remove HPR"
    echo "  4) Exit"
    echo -e "${BOLD}=================================================${NC}"
    read -p "Select an action (1-4): " choice < /dev/tty
    echo ""
    
    case "$choice" in
        1)
            install_hpr
            ;;
        2)
            update_hpr
            ;;
        3)
            remove_hpr
            ;;
        4)
            echo "Exiting HPR Installation Manager. Goodbye!"
            exit 0
            ;;
        *)
            echo -e "${RED}Invalid option. Please choose between 1 and 4.${NC}"
            echo ""
            ;;
    esac
done
