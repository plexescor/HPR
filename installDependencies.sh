#!/usr/bin/env bash

set -euo pipefail

SLINT_VERSION="1.16.1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXTERNAL_DIR="${SCRIPT_DIR}/external"

# ── Install prefix: system-wide if root, user-local otherwise ─────────────────
if [[ $EUID -eq 0 ]]; then
    INSTALL_MODE="system"
    DEST_BASE="/usr/local"
else
    INSTALL_MODE="user"
    DEST_BASE="${HOME}/.local"
fi

DEST_BIN="${DEST_BASE}/bin"
DEST_INC="${DEST_BASE}/include"
DEST_LIB="${DEST_BASE}/lib"
DEST_CMAKE="${DEST_LIB}/cmake"

# ── Colors ────────────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()    { echo -e "${CYAN}[INFO]${NC}  $*"; }
success() { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*" >&2; exit 1; }

require_cmd() {
    command -v "$1" &>/dev/null || error "Missing command: $1"
}

require_cmd curl
require_cmd tar

# ── Print mode ────────────────────────────────────────────────────────────────
if [[ "${INSTALL_MODE}" == "system" ]]; then
    info "Running as root — installing system-wide:"
else
    info "Running as user — installing to user-local prefix:"
fi
echo "  BIN   -> ${DEST_BIN}"
echo "  INC   -> ${DEST_INC}"
echo "  LIB   -> ${DEST_LIB}"
echo ""

mkdir -p \
    "${EXTERNAL_DIR}" \
    "${DEST_BIN}" \
    "${DEST_INC}" \
    "${DEST_LIB}" \
    "${DEST_CMAKE}"

# ── URLs ──────────────────────────────────────────────────────────────────────
declare -A URLS=(
    ["Slint-cpp-${SLINT_VERSION}-Linux-x86_64.tar.gz"]="https://github.com/slint-ui/slint/releases/download/v${SLINT_VERSION}/Slint-cpp-${SLINT_VERSION}-Linux-x86_64.tar.gz"
    ["slint-lsp-linux.tar.gz"]="https://github.com/slint-ui/slint/releases/download/v${SLINT_VERSION}/slint-lsp-linux.tar.gz"
    ["slint-viewer-linux.tar.gz"]="https://github.com/slint-ui/slint/releases/download/v${SLINT_VERSION}/slint-viewer-linux.tar.gz"
)

# ── Download ──────────────────────────────────────────────────────────────────
for filename in "${!URLS[@]}"; do
    dest="${EXTERNAL_DIR}/${filename}"
    url="${URLS[$filename]}"

    if [[ ! -f "${dest}" ]]; then
        info "Downloading ${filename}"
        curl -fL --progress-bar -o "${dest}" "${url}"
    else
        warn "Already downloaded: ${filename}"
    fi
done

# ── Extract ───────────────────────────────────────────────────────────────────
CPP_ARCHIVE="${EXTERNAL_DIR}/Slint-cpp-${SLINT_VERSION}-Linux-x86_64.tar.gz"
CPP_EXTRACT_DIR="${EXTERNAL_DIR}/Slint-cpp-${SLINT_VERSION}-Linux-x86_64"

if [[ ! -d "${CPP_EXTRACT_DIR}" ]]; then
    info "Extracting Slint C++ SDK"
    tar -xzf "${CPP_ARCHIVE}" -C "${EXTERNAL_DIR}/"
fi

LSP_EXTRACT_DIR="${EXTERNAL_DIR}/slint-lsp"
if [[ ! -d "${LSP_EXTRACT_DIR}" ]]; then
    mkdir -p "${LSP_EXTRACT_DIR}"
    info "Extracting slint-lsp"
    tar -xzf "${EXTERNAL_DIR}/slint-lsp-linux.tar.gz" -C "${LSP_EXTRACT_DIR}/"
fi

VIEWER_EXTRACT_DIR="${EXTERNAL_DIR}/slint-viewer"
if [[ ! -d "${VIEWER_EXTRACT_DIR}" ]]; then
    mkdir -p "${VIEWER_EXTRACT_DIR}"
    info "Extracting slint-viewer"
    tar -xzf "${EXTERNAL_DIR}/slint-viewer-linux.tar.gz" -C "${VIEWER_EXTRACT_DIR}/"
fi

# ── Install SDK headers ───────────────────────────────────────────────────────
if [[ -d "${CPP_EXTRACT_DIR}/include" ]]; then
    info "Installing headers"
    cp -r "${CPP_EXTRACT_DIR}/include/." "${DEST_INC}/"
fi

# ── Install libs + CMake config ───────────────────────────────────────────────
if [[ -d "${CPP_EXTRACT_DIR}/lib" ]]; then
    info "Installing libraries"
    find "${CPP_EXTRACT_DIR}/lib" -maxdepth 1 \
        \( -name "*.so*" -o -name "*.a" \) \
        -exec cp -P {} "${DEST_LIB}/" \;

    if [[ -d "${CPP_EXTRACT_DIR}/lib/cmake" ]]; then
        mkdir -p "${DEST_CMAKE}"
        cp -r "${CPP_EXTRACT_DIR}/lib/cmake/." "${DEST_CMAKE}/"
    fi
fi

# ── Install binaries ──────────────────────────────────────────────────────────
for sdk_bin in slint-compiler; do
    bin_path="$(find "${CPP_EXTRACT_DIR}/bin" -maxdepth 1 -type f -name "${sdk_bin}" | head -n1)"
    if [[ -n "${bin_path}" ]]; then
        info "Installing ${sdk_bin}"
        install -Dm755 "${bin_path}" "${DEST_BIN}/${sdk_bin}"
    fi
done

LSP_BIN="$(find "${LSP_EXTRACT_DIR}" -type f -name "slint-lsp" | head -n1)"
install -Dm755 "${LSP_BIN}" "${DEST_BIN}/slint-lsp"

VIEWER_BIN="$(find "${VIEWER_EXTRACT_DIR}" -type f -name "slint-viewer" | head -n1)"
install -Dm755 "${VIEWER_BIN}" "${DEST_BIN}/slint-viewer"

# ── Done ──────────────────────────────────────────────────────────────────────
echo ""
success "Slint ${SLINT_VERSION} installed (${INSTALL_MODE})."
echo ""

if [[ "${INSTALL_MODE}" == "user" ]]; then
    echo "Add these to your shell profile (~/.bashrc / ~/.zshrc) if not already there:"
    echo ""
    echo '  export PATH="$HOME/.local/bin:$PATH"'
    echo '  export LD_LIBRARY_PATH="$HOME/.local/lib:$LD_LIBRARY_PATH"'
    echo ""
    echo "Then configure CMake with:"
    echo ""
    echo "  cmake -B build -DCMAKE_PREFIX_PATH=\"\$HOME/.local\""
    echo ""
else
    echo "CMake should find Slint automatically (system-wide install)."
    echo ""
    echo "Configure CMake with:"
    echo ""
    echo "  cmake -B build"
    echo ""
fi