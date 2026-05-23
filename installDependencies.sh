#!/usr/bin/env bash

set -euo pipefail

SLINT_VERSION="1.16.1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXTERNAL_DIR="${SCRIPT_DIR}/external"

# User-local install
DEST_BASE="${HOME}/.local"
DEST_BIN="${DEST_BASE}/bin"
DEST_INC="${DEST_BASE}/include"
DEST_LIB="${DEST_BASE}/lib"
DEST_CMAKE="${DEST_LIB}/cmake"

declare -A URLS=(
    ["Slint-cpp-${SLINT_VERSION}-Linux-x86_64.tar.gz"]="https://github.com/slint-ui/slint/releases/download/v${SLINT_VERSION}/Slint-cpp-${SLINT_VERSION}-Linux-x86_64.tar.gz"
    ["slint-lsp-linux.tar.gz"]="https://github.com/slint-ui/slint/releases/download/v${SLINT_VERSION}/slint-lsp-linux.tar.gz"
    ["slint-viewer-linux.tar.gz"]="https://github.com/slint-ui/slint/releases/download/v${SLINT_VERSION}/slint-viewer-linux.tar.gz"
)

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

mkdir -p \
    "${EXTERNAL_DIR}" \
    "${DEST_BIN}" \
    "${DEST_INC}" \
    "${DEST_LIB}" \
    "${DEST_CMAKE}"

info "Using user-local install:"
echo "  BIN   -> ${DEST_BIN}"
echo "  INC   -> ${DEST_INC}"
echo "  LIB   -> ${DEST_LIB}"

# Download
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

# Extract
CPP_ARCHIVE="${EXTERNAL_DIR}/Slint-cpp-${SLINT_VERSION}-Linux-x86_64.tar.gz"
CPP_EXTRACT_DIR="${EXTERNAL_DIR}/Slint-cpp-${SLINT_VERSION}-Linux-x86_64"

if [[ ! -d "${CPP_EXTRACT_DIR}" ]]; then
    tar -xzf "${CPP_ARCHIVE}" -C "${EXTERNAL_DIR}/"
fi

LSP_EXTRACT_DIR="${EXTERNAL_DIR}/slint-lsp"
mkdir -p "${LSP_EXTRACT_DIR}"
tar -xzf "${EXTERNAL_DIR}/slint-lsp-linux.tar.gz" -C "${LSP_EXTRACT_DIR}/"

VIEWER_EXTRACT_DIR="${EXTERNAL_DIR}/slint-viewer"
mkdir -p "${VIEWER_EXTRACT_DIR}"
tar -xzf "${EXTERNAL_DIR}/slint-viewer-linux.tar.gz" -C "${VIEWER_EXTRACT_DIR}/"

# Install SDK headers
if [[ -d "${CPP_EXTRACT_DIR}/include" ]]; then
    cp -r "${CPP_EXTRACT_DIR}/include/." "${DEST_INC}/"
fi

# Install libs
if [[ -d "${CPP_EXTRACT_DIR}/lib" ]]; then
    find "${CPP_EXTRACT_DIR}/lib" -maxdepth 1 \
        \( -name "*.so*" -o -name "*.a" \) \
        -exec cp -P {} "${DEST_LIB}/" \;

    if [[ -d "${CPP_EXTRACT_DIR}/lib/cmake" ]]; then
        mkdir -p "${DEST_CMAKE}"
        cp -r "${CPP_EXTRACT_DIR}/lib/cmake/." "${DEST_CMAKE}/"
    fi
fi

# Install binaries
for sdk_bin in slint-compiler; do
    bin_path="$(find "${CPP_EXTRACT_DIR}/bin" -maxdepth 1 -type f -name "${sdk_bin}" | head -n1)"

    if [[ -n "${bin_path}" ]]; then
        install -Dm755 "${bin_path}" "${DEST_BIN}/${sdk_bin}"
    fi
done

LSP_BIN="$(find "${LSP_EXTRACT_DIR}" -type f -name "slint-lsp" | head -n1)"
install -Dm755 "${LSP_BIN}" "${DEST_BIN}/slint-lsp"

VIEWER_BIN="$(find "${VIEWER_EXTRACT_DIR}" -type f -name "slint-viewer" | head -n1)"
install -Dm755 "${VIEWER_BIN}" "${DEST_BIN}/slint-viewer"

echo ""
success "Slint installed locally."

echo ""
echo "Add these if needed:"
echo 'export PATH="$HOME/.local/bin:$PATH"'
echo 'export LD_LIBRARY_PATH="$HOME/.local/lib:$LD_LIBRARY_PATH"'