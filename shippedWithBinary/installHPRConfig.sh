#!/bin/bash

CONFIG_DIR="$HOME/.config/HPR"
DATA_DIR="$HOME/.local/share/HPR"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FORCE=false

if [[ "$1" == "--force" ]]; then
    FORCE=true
    echo "================================================"
    echo "       HPR Configuration Installer"
    echo "================================================"
    echo ""
    echo "!! WARNING: --force is set. This will overwrite your"
    echo "   existing config files with the default shipped files."
    echo "   Any changes YOU made to aliases.csv or config.csv"
    echo "   will be permanently lost!"
    echo ""
    read -p "   Are you sure? (y/N): " confirm
    if [[ "$confirm" != "y" && "$confirm" != "Y" ]]; then
        echo "   Aborted. Your config files are untouched."
        exit 0
    fi
    echo ""
else
    echo "================================================"
    echo "       HPR Configuration Installer"
    echo "================================================"
    echo ""
    echo "This script will copy HPR's required config files"
    echo "to the correct locations on your system."
    echo ""
fi

# Create dirs
echo ">> Creating config directories if they don't exist..."
mkdir -p "$CONFIG_DIR"
mkdir -p "$DATA_DIR"
echo "   Config dir : $CONFIG_DIR"
echo "   Data dir   : $DATA_DIR"
echo ""

# Check and copy aliases.csv
if [[ -f "$CONFIG_DIR/aliases.csv" ]] && [[ "$FORCE" == false ]]; then
    echo ">> aliases.csv already exists — skipping."
    echo "   (run with --force to overwrite, but you will lose your edits!)"
else
    cp "$SCRIPT_DIR/aliases.csv" "$CONFIG_DIR/aliases.csv"
    echo ">> aliases.csv copied successfully."
fi

echo ""

# Check and copy config.csv
if [[ -f "$CONFIG_DIR/config.csv" ]] && [[ "$FORCE" == false ]]; then
    echo ">> config.csv already exists — skipping."
    echo "   (run with --force to overwrite, but you will lose your edits!)"
else
    cp "$SCRIPT_DIR/config.csv" "$CONFIG_DIR/config.csv"
    echo ">> config.csv copied successfully."
fi

echo ""
echo "================================================"
echo "  Done! You can now run HPR."
echo ""
echo "  Your config files are located at:"
echo "  $CONFIG_DIR"
echo ""
echo "  To reset config to defaults, run:"
echo "  ./copyHPRConfig.sh --force"
echo "  (WARNING: this will overwrite your edits!)"
echo "================================================"