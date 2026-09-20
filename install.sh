#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Define color codes for pretty output
BOLD='\033[1m'
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
} >&2
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
        echo -e "${RED}Error: The following required tools are missing and must be installed before continuing:${NC}"
        for dep in "${MISSING_DEPS[@]}"; do
            echo "  - $dep"
        done
        echo "   Install them with your package manager and run the installer again."
        echo "   Examples: apt install <name>  |  pacman -S <name>  |  dnf install <name>"
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
        echo -e "${RED}Error: Could not fetch the latest HPR release from GitHub.${NC}" >&2
        echo "   This may be caused by a network issue or a GitHub API rate limit."
        echo "   Check your internet connection and try again."
        echo "   If the problem persists, choose 'Custom version' to install a specific release."
        exit 1
    fi
    
    VERSION_NUM=$(echo "$TAG_NAME" | sed 's/^v//')
    echo "   Latest version: v$VERSION_NUM (tag: $TAG_NAME)"
    echo ""
}

select_version() {
    # Always fetch latest release information first to inform the user
    fetch_latest_version

    echo -e "${BOLD}Select HPR version to install/update:${NC}"
    echo "  1) Latest release ($TAG_NAME)"
    echo "  2) Custom version"
    
    while true; do
        read -p "Select option (1-2): " ver_choice < /dev/tty
        case "$ver_choice" in
            1)
                # Already fetched in the beginning
                break
                ;;
            2)
                while true; do
                    echo ">> Enter the HPR version tag you want to install."
                    echo "   Find all available releases at: https://github.com/plexescor/HPR/releases"
                    echo -e "   (Example: ${BOLD}v0.9.3${NC} or ${BOLD}0.9.3${NC} — the leading 'v' is optional)"
                    read -p "Version: " custom_ver < /dev/tty
                    
                    # Trim whitespace using sed
                    custom_ver=$(echo "$custom_ver" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
                    
                    if [ -z "$custom_ver" ]; then
                        echo -e "${RED}Error: Version cannot be empty. Please enter a valid version tag (e.g. v0.9.3).${NC}"
                        continue
                    fi
                    
                    # Normalize: if "v" is there proceed as usual, otherwise prepend "v"
                    if [[ "$custom_ver" =~ ^v ]]; then
                        TAG_NAME="$custom_ver"
                        VERSION_NUM="${custom_ver#v}"
                    else
                        TAG_NAME="v$custom_ver"
                        VERSION_NUM="$custom_ver"
                    fi
                    
                    echo "   Selected version: $TAG_NAME"
                    echo ""
                    break 2
                done
                ;;
            *)
                echo -e "${RED}Invalid option. Please choose 1 or 2.${NC}"
                ;;
        esac
    done
}

download_and_extract() {
    cleanup_temp
    TEMP_DIR=$(mktemp -d -t hpr-installer-XXXXXX)
    
    local ASSET_NAME="HPRv${VERSION_NUM}-Linux.tar.xz"
    local ASSET_URL="https://github.com/plexescor/HPR/releases/download/${TAG_NAME}/${ASSET_NAME}"
    local DOWNLOAD_SUCCESS=false
    local USE_ZIP=false
    
    echo ">> Downloading release archive from:" >&2
    echo "   $ASSET_URL" >&2
    if curl -fL --progress-bar -o "$TEMP_DIR/$ASSET_NAME" "$ASSET_URL"; then
        DOWNLOAD_SUCCESS=true
    else
        echo ">> .tar.xz archive not found or download failed. Trying zip fallback..." >&2
        local ASSET_NAME_ZIP="HPRv${VERSION_NUM}-Linux.zip"
        local ASSET_URL_ZIP="https://github.com/plexescor/HPR/releases/download/${TAG_NAME}/${ASSET_NAME_ZIP}"
        echo ">> Downloading release archive from:" >&2
        echo "   $ASSET_URL_ZIP" >&2
        if curl -fL --progress-bar -o "$TEMP_DIR/$ASSET_NAME_ZIP" "$ASSET_URL_ZIP"; then
            DOWNLOAD_SUCCESS=true
            USE_ZIP=true
        fi
    fi
    
    if [ "$DOWNLOAD_SUCCESS" = false ]; then
        echo -e "${RED}Error: Failed to download the release archive (.tar.xz or .zip).${NC}" >&2
        echo "   Possible causes:" >&2
        echo "     - The version tag '$TAG_NAME' does not exist. Check https://github.com/plexescor/HPR/releases for valid tags." >&2
        echo "     - A network error or firewall is blocking the download." >&2
        exit 1
    fi
    
    echo ">> Extracting release archive..." >&2
    if [ "$USE_ZIP" = true ]; then
        if ! command -v unzip >/dev/null 2>&1; then
            echo -e "${RED}Error: 'unzip' is required to extract the zip fallback but is not installed.${NC}" >&2
            echo "   Please install 'unzip' using your package manager and try again." >&2
            exit 1
        fi
        unzip -q "$TEMP_DIR/$ASSET_NAME_ZIP" -d "$TEMP_DIR"
    else
        tar -xf "$TEMP_DIR/$ASSET_NAME" -C "$TEMP_DIR"
    fi
    echo "   Extraction complete." >&2
    
    # Locate the extracted source directory containing HPR
    if [ -f "$TEMP_DIR/HPR" ]; then
        SRC_DIR="$TEMP_DIR"
    else
        SRC_DIR=$(find "$TEMP_DIR" -type f -name "HPR" -exec dirname {} \; | head -n 1)
    fi
    
    if [ -z "$SRC_DIR" ] || [ ! -d "$SRC_DIR" ]; then
        echo -e "${RED}Error: Could not locate the HPR binary inside the downloaded archive.${NC}" >&2
        echo "   The archive may be malformed or from an unexpected release format." >&2
        echo "   Please try again or report this issue at: https://github.com/plexescor/HPR/issues" >&2
        exit 1
    fi
}

needs_sudo() {
    local target="$1"
    while [ ! -e "$target" ] && [ "$target" != "/" ]; do
        target=$(dirname "$target")
    done
    if [ -d "$target" ]; then
        [ ! -w "$target" ]
    else
        [ ! -w "$target" ] || [ ! -w "$(dirname "$target")" ]
    fi
}

needs_sudo_for_removal() {
    local target="$1"
    while [ ! -e "$target" ] && [ "$target" != "/" ]; do
        target=$(dirname "$target")
    done
    [ ! -w "$target" ] || [ ! -w "$(dirname "$target")" ]
}

is_protected_dir() {
    local dir="$1"
    while [[ "$dir" == */ && "$dir" != "/" ]]; do
        dir="${dir%/}"
    done
    case "$dir" in
        "/"|"/bin"|"/usr"|"/usr/bin"|"/usr/local"|"/usr/local/bin"|"/sbin"|"/usr/sbin"|"/etc"|"/opt"|"/var"|"/home")
            return 0
            ;;
        "$HOME"|"$HOME/Applications"|"$HOME/.local"|"$HOME/.local/bin"|"$HOME/bin"|"$HOME/Desktop"|"$HOME/Downloads"|"$HOME/Documents"|"$HOME/.config")
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

has_non_hpr_files() {
    local dir="$1"
    [ -d "$dir" ] || return 1
    find "$dir" -mindepth 1 -maxdepth 1 ! -name "HPR" ! -name "hpr" ! -name "libslint_cpp.so*" 2>/dev/null | grep -q .
}

write_metadata_file() {
    local target_path="$1"
    echo ">> Saving installation path for future updates and removal..."
    
    # Write to primary system metadata if installing to a system path requiring sudo
    if needs_sudo "$target_path"; then
        if sudo mkdir -p "$(dirname "$METADATA_FILE")" >/dev/null 2>&1; then
            if echo "$target_path" | sudo tee "$METADATA_FILE" >/dev/null; then
                echo "   Saved path to primary location: $METADATA_FILE"
                return 0
            fi
        fi
    fi
    
    # Fallback or user-space path
    mkdir -p "$(dirname "$FALLBACK_METADATA_FILE")"
    echo "$target_path" > "$FALLBACK_METADATA_FILE"
    echo "   Saved path to location: $FALLBACK_METADATA_FILE"
}

remove_metadata_files() {
    local target_path="$1"
    echo ">> Removing metadata files..."
    if [ -f "$METADATA_FILE" ]; then
        local meta_val
        meta_val=$(cat "$METADATA_FILE" 2>/dev/null || true)
        if [ -z "$target_path" ] || [ "$meta_val" = "$target_path" ]; then
            local SUDO=""
            if needs_sudo "$METADATA_FILE"; then
                SUDO="sudo"
            fi
            $SUDO rm -f "$METADATA_FILE"
            echo "   Removed primary metadata file: $METADATA_FILE"
        fi
    fi
    if [ -f "$FALLBACK_METADATA_FILE" ]; then
        local meta_val
        meta_val=$(cat "$FALLBACK_METADATA_FILE" 2>/dev/null || true)
        if [ -z "$target_path" ] || [ "$meta_val" = "$target_path" ]; then
            rm -f "$FALLBACK_METADATA_FILE"
            echo "   Removed fallback metadata file: $FALLBACK_METADATA_FILE"
        fi
    fi
}

check_if_already_installed() {
    local PATH_FOUND=""
    if [ -f "$METADATA_FILE" ]; then
        PATH_FOUND=$(cat "$METADATA_FILE" 2>/dev/null || true)
        if [ -n "$PATH_FOUND" ] && [ -f "$PATH_FOUND" ]; then
            return 0
        fi
    fi
    if [ -f "$FALLBACK_METADATA_FILE" ]; then
        PATH_FOUND=$(cat "$FALLBACK_METADATA_FILE" 2>/dev/null || true)
        if [ -n "$PATH_FOUND" ] && [ -f "$PATH_FOUND" ]; then
            return 0
        fi
    fi
    return 1
}

get_hpr_path() {
    local PATH_FOUND=""
    
    # 1. Check primary metadata file
    if [ -f "$METADATA_FILE" ]; then
        PATH_FOUND=$(cat "$METADATA_FILE" 2>/dev/null || true)
        if [ -n "$PATH_FOUND" ] && [ -f "$PATH_FOUND" ]; then
            echo "$PATH_FOUND"
            return 0
        fi
    fi
    
    # 2. Check fallback metadata file if primary not found or invalid
    if [ -f "$FALLBACK_METADATA_FILE" ]; then
        PATH_FOUND=$(cat "$FALLBACK_METADATA_FILE" 2>/dev/null || true)
        if [ -n "$PATH_FOUND" ] && [ -f "$PATH_FOUND" ]; then
            echo "$PATH_FOUND"
            return 0
        fi
    fi

    
    # If not found, ask user
    echo -e "${YELLOW}>> HPR installation was not detected automatically.${NC}" >&2
    echo "   If you installed HPR manually or moved the binary, enter the full path to it below." >&2
    echo "   The path must point to the HPR binary itself, not its containing folder." >&2
    echo "   (Example: /home/username/HPR_DIR/HPR)" >&2
    echo "" >&2
    while true; do
        read -p ">> Path to HPR binary (press [Enter] if HPR is not installed): " input_path < /dev/tty
        
        if [ -z "$input_path" ]; then
            echo "   No path provided. Returning to the main menu." >&2
            echo "   Use 'Install HPR' (option 1) to install it first." >&2
            echo "" >&2
            return 1
        fi
        
        # Check for tilde
        if [[ "$input_path" == *~* ]]; then
            echo -e "${RED}Error: Do not use ~ in the path — the shell does not expand it here.${NC}" >&2
            echo "   Use the full absolute path. (e.g. /home/your-username/HPR_DIR/HPR)" >&2
            echo "" >&2
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
            echo "   Path not confirmed. Please try again." >&2
            echo "" >&2
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
    
    # Always update reference
    if [ -n "$UI_SRC" ]; then
        if [ ! -d "$CONFIG_DIR/ui-REFERENCEONLY" ] || ! diff -r "$UI_SRC" "$CONFIG_DIR/ui-REFERENCEONLY" >/dev/null 2>&1; then
            echo ">> Updating ui-REFERENCEONLY to latest..."
            rm -rf "$CONFIG_DIR/ui-REFERENCEONLY"
            cp -r "$UI_SRC" "$CONFIG_DIR/ui-REFERENCEONLY"
        fi
    fi

    # Always overwrite active ui/ folder
    if [ -n "$UI_SRC" ]; then
        echo ">> Updating active UI folder to latest..."
        rm -rf "$CONFIG_DIR/ui"
        cp -r "$UI_SRC" "$CONFIG_DIR/ui"
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
            echo -e "${YELLOW}>> GNOME Desktop detected. HPR requires a small Shell extension to track which window is active.${NC}"
            echo "   Extension: lol-another-window-extension (github.com/plexescor/lol-another-window-extension)"
            echo "   Without it, HPR cannot detect active windows on GNOME Wayland."
            echo ""
            read -p ">> Install the GNOME Shell extension now? Answering N will skip it — HPR will still launch but cannot track windows on GNOME Wayland. (Y/n): " install_ext < /dev/tty
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
                            echo -e "${BOLD}   Extension enabled successfully! No restart needed.${NC}"
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
                    echo -e "${RED}Error: Failed to download the GNOME Shell extension from GitHub.${NC}"
                    echo "   Check your internet connection and try again."
                    echo "   You can also install it manually from:"
                    echo "   https://github.com/plexescor/lol-another-window-extension"
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
    if check_if_already_installed; then
        echo -e "${YELLOW}HPR is already installed on this system.${NC}"
        echo "   To upgrade to a newer version, use option 2) Update HPR."
        echo "   To change your installation location, use option 2) Update HPR."
        echo ""
        return 0
    fi

    echo ">> Initiating HPR Installation process..."
    
    # 1. Dependency Verification
    check_dependencies
    
    # 2. HPR Version Selection
    select_version
    
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
    
    echo ">> Choose install location."
    echo "   The HPR binary will be installed here. sudo access is required to write to system paths."
    echo "   Default: $INSTALL_PATH"
    echo "   Or enter a custom absolute path (e.g. /home/username/apps/HPR). Press Enter to use the default."
    while true; do
        read -p "   Install path: " input_path < /dev/tty
        if [ -z "$input_path" ]; then
            break
        fi
        if [[ "$input_path" == *~* ]]; then
            echo -e "${RED}Error: Do not use ~ in the path — the shell does not expand it here.${NC}" >&2
            echo "   Use the full absolute path. (e.g. /home/your-username/apps/HPR)"
            continue
        fi
        
        # Process custom path: strip trailing slashes to clean it up
        local cleaned_path="$input_path"
        while [[ "$cleaned_path" == */ && "$cleaned_path" != "/" ]]; do
            cleaned_path="${cleaned_path%/}"
        done
        
        local parent_dir
        parent_dir=$(dirname "$cleaned_path")
        local base_name
        base_name=$(basename "$cleaned_path")

        # Standard bin directory (e.g. /usr/local/bin or ~/.local/bin)
        if [[ "$cleaned_path" == */bin ]]; then
            INSTALL_PATH="$cleaned_path/HPR"
        elif [[ "$parent_dir" == */bin && ("$base_name" == "HPR" || "$base_name" == "hpr") ]]; then
            INSTALL_PATH="$cleaned_path"
        else
            # For custom application directories, install into a dedicated HPR subfolder
            if [[ "$base_name" == "HPR" || "$base_name" == "hpr" ]]; then
                if [[ "$(basename "$parent_dir")" == "HPR" || "$(basename "$parent_dir")" == "hpr" ]]; then
                    INSTALL_PATH="$cleaned_path"
                else
                    INSTALL_PATH="$cleaned_path/HPR"
                fi
            else
                INSTALL_PATH="$cleaned_path/HPR/HPR"
            fi
        fi
        break
    done
    
    local INSTALL_DIR
    INSTALL_DIR=$(dirname "$INSTALL_PATH")

    local SUDO=""
    if needs_sudo "$INSTALL_DIR" || needs_sudo "$INSTALL_PATH"; then
        SUDO="sudo"
    fi

    if [ ! -d "$INSTALL_DIR" ]; then
        echo ">> Creating target directory: $INSTALL_DIR..."
        $SUDO mkdir -p "$INSTALL_DIR"
    fi
    
    echo ">> Installing HPR binary to $INSTALL_PATH..."
    if [ -f "$INSTALL_PATH" ]; then
        $SUDO rm -f "$INSTALL_PATH"
    fi
    $SUDO install -m 755 "$SRC_DIR/HPR" "$INSTALL_PATH"
    echo "   Binary installed successfully."
    
    # Install any dynamic libraries (*.so)
    echo ">> Installing dynamic libraries to $INSTALL_DIR..."
    find "$SRC_DIR" -name "*.so*" | while read -r so_file; do
        if [ -e "$so_file" ] || [ -L "$so_file" ]; then
            local target_so="$INSTALL_DIR/$(basename "$so_file")"
            if [ -e "$target_so" ] || [ -L "$target_so" ]; then
                $SUDO rm -f "$target_so"
            fi
            $SUDO cp -d "$so_file" "$target_so"
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
    echo -e "${BOLD}=================================================${NC}"
    echo -e "${BOLD}HPR Installation Complete!${NC}"
    echo "   HPR has been installed to: $INSTALL_PATH"
    echo "   You can run it from a terminal: HPR"
    echo "   Or find it in your application launcher."
    echo -e "${BOLD}=================================================${NC}"
    echo ""
}

update_hpr() {
    local current_bin_path
    current_bin_path=$(get_hpr_path) || return 0 # Return to menu if HPR not found/not installed
    
    echo ">> Initiating HPR Update process..."
    
    # 1. Check dependencies
    check_dependencies
    
    # 2. HPR Version Selection
    select_version
    
    # 3. Download and extract
    download_and_extract
    
    # Determine directory containing HPR
    local INSTALL_PATH="$current_bin_path"
    local INSTALL_DIR
    INSTALL_DIR=$(dirname "$INSTALL_PATH")

    local SUDO=""
    if needs_sudo "$INSTALL_DIR" || needs_sudo "$INSTALL_PATH"; then
        SUDO="sudo"
    fi
    
    echo ">> Preparing update: replacing old HPR files in '$INSTALL_DIR'..."
    echo "   Removing old HPR binary and shared library (libslint_cpp.so)..."
    $SUDO rm -f "$INSTALL_PATH"
    $SUDO rm -f "$INSTALL_DIR"/libslint_cpp.so*
    
    echo ">> Installing HPR binary to $INSTALL_PATH..."
    $SUDO install -m 755 "$SRC_DIR/HPR" "$INSTALL_PATH"
    
    echo ">> Installing dynamic libraries to $INSTALL_DIR..."
    find "$SRC_DIR" -name "*.so*" | while read -r so_file; do
        if [ -e "$so_file" ] || [ -L "$so_file" ]; then
            local target_so="$INSTALL_DIR/$(basename "$so_file")"
            if [ -e "$target_so" ] || [ -L "$target_so" ]; then
                $SUDO rm -f "$target_so"
            fi
            $SUDO cp -d "$so_file" "$target_so"
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
    echo -e "${BOLD}=================================================${NC}"
    echo -e "${BOLD}HPR Update Complete!${NC}"
    echo "   HPR $TAG_NAME has been installed to: $INSTALL_PATH"
    echo -e "${BOLD}=================================================${NC}"
    echo ""
}

remove_hpr() {
    local current_bin_path
    current_bin_path=$(get_hpr_path) || return 0 # Return to menu if HPR not found/not installed
    
    echo ">> Initiating HPR Removal process..."
    
    local INSTALL_PATH="$current_bin_path"
    local INSTALL_DIR
    INSTALL_DIR=$(dirname "$INSTALL_PATH")
    local install_base
    install_base=$(basename "$INSTALL_DIR")
    if is_protected_dir "$INSTALL_DIR" || has_non_hpr_files "$INSTALL_DIR" || [[ "$install_base" != "HPR" && "$install_base" != "hpr" ]]; then
        local SUDO=""
        if needs_sudo "$INSTALL_DIR" || needs_sudo "$INSTALL_PATH"; then
            SUDO="sudo"
        fi
        echo "   Note: '$INSTALL_DIR' is a protected or shared path — only the HPR binary and its libraries will be removed, not the whole directory."
        echo "   Removing HPR binary and shared library (libslint_cpp.so)..."
        $SUDO rm -f "$INSTALL_PATH"
        $SUDO rm -f "$INSTALL_DIR"/libslint_cpp.so*
    else
        local SUDO=""
        if needs_sudo_for_removal "$INSTALL_DIR" || needs_sudo "$INSTALL_PATH"; then
            SUDO="sudo"
        fi
        echo -e "${RED}WARNING: '$INSTALL_DIR' is a dedicated HPR directory and will be removed.${NC}"
        read -p "$(echo -e "${RED}Confirm deletion of directory '$INSTALL_DIR'? (y/N): ${NC}")" confirm_wipe < /dev/tty
        if [[ "$confirm_wipe" =~ ^[Yy] ]]; then
            echo "   Removing directory: $INSTALL_DIR..."
            $SUDO rm -rf "$INSTALL_DIR"
        else
            echo "   Cancelled. No files were deleted. Returning to the main menu."
            echo ""
            return 0
        fi
    fi
    
    # Remove desktop launcher files and update caches
    remove_desktop_launcher
    
    # Remove metadata
    remove_metadata_files "$INSTALL_PATH"
    
    echo -e "${BOLD}=================================================${NC}"
    echo -e "${BOLD}HPR Removal Complete!${NC}"
    echo "   The HPR binary and installer metadata have been removed."
    echo "   Your personal config files in ~/.config/HPR were NOT deleted."
    echo "   To remove them manually: rm -rf ~/.config/HPR"
    echo -e "${BOLD}=================================================${NC}"
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
done >&2
