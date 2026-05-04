#!/usr/bin/env bash
# =============================================================================
# installDependencies.sh
# Downloads Slint v1.16.1 release artifacts, extracts them into ./external/,
# and installs them system-wide:
#   binaries  -> /usr/local/bin/
#   headers   -> /usr/local/include/
#   libs      -> /usr/local/lib/
#   cmake     -> /usr/local/lib/cmake/
# Must be run with sudo (or as root) for the install step.
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
SLINT_VERSION="1.16.1"
EXTERNAL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/external"

# Explicit install destinations (matches observed system layout)
DEST_BIN="/usr/local/bin"
DEST_INC="/usr/local/include"
DEST_LIB="/usr/local/lib"
DEST_CMAKE="/usr/local/lib/cmake"

declare -A URLS=(
    ["Slint-cpp-${SLINT_VERSION}-Linux-x86_64.tar.gz"]="https://github.com/slint-ui/slint/releases/download/v${SLINT_VERSION}/Slint-cpp-${SLINT_VERSION}-Linux-x86_64.tar.gz"
    ["slint-lsp-linux.tar.gz"]="https://github.com/slint-ui/slint/releases/download/v${SLINT_VERSION}/slint-lsp-linux.tar.gz"
    ["slint-viewer-linux.tar.gz"]="https://github.com/slint-ui/slint/releases/download/v${SLINT_VERSION}/slint-viewer-linux.tar.gz"
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'

info()    { echo -e "${CYAN}[INFO]${NC}  $*"; }
success() { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*" >&2; exit 1; }

require_cmd() { command -v "$1" &>/dev/null || error "Required command not found: $1"; }

# ---------------------------------------------------------------------------
# Preflight checks
# ---------------------------------------------------------------------------
require_cmd curl
require_cmd tar

if [[ $EUID -ne 0 ]]; then
    error "This script must be run as root (use: sudo ./installDependencies.sh)"
fi

# ---------------------------------------------------------------------------
# Step 1 – Prepare external/ directory
# ---------------------------------------------------------------------------
info "Preparing external directory: ${EXTERNAL_DIR}"
mkdir -p "${EXTERNAL_DIR}"

# ---------------------------------------------------------------------------
# Step 2 – Download archives
# ---------------------------------------------------------------------------
info "Downloading Slint v${SLINT_VERSION} release artifacts..."
for filename in "${!URLS[@]}"; do
    dest="${EXTERNAL_DIR}/${filename}"
    url="${URLS[$filename]}"

    if [[ -f "${dest}" ]]; then
        warn "Already exists, skipping download: ${filename}"
    else
        info "  Downloading ${filename} ..."
        curl -fL --progress-bar -o "${dest}" "${url}" \
            || error "Download failed for: ${url}"
        success "  Saved: ${dest}"
    fi
done

# ---------------------------------------------------------------------------
# Step 3 – Extract archives
# ---------------------------------------------------------------------------
info "Extracting archives into ${EXTERNAL_DIR}/ ..."

# --- Slint C++ SDK -----------------------------------------------------------
CPP_ARCHIVE="${EXTERNAL_DIR}/Slint-cpp-${SLINT_VERSION}-Linux-x86_64.tar.gz"
CPP_EXTRACT_DIR="${EXTERNAL_DIR}/Slint-cpp-${SLINT_VERSION}-Linux-x86_64"

if [[ ! -d "${CPP_EXTRACT_DIR}" ]]; then
    info "  Extracting Slint C++ SDK..."
    tar -xzf "${CPP_ARCHIVE}" -C "${EXTERNAL_DIR}/"
    success "  Extracted to: ${CPP_EXTRACT_DIR}"
else
    warn "  Already extracted: ${CPP_EXTRACT_DIR}"
fi

# --- slint-lsp ---------------------------------------------------------------
LSP_ARCHIVE="${EXTERNAL_DIR}/slint-lsp-linux.tar.gz"
LSP_EXTRACT_DIR="${EXTERNAL_DIR}/slint-lsp"

if [[ ! -d "${LSP_EXTRACT_DIR}" ]]; then
    info "  Extracting slint-lsp..."
    mkdir -p "${LSP_EXTRACT_DIR}"
    tar -xzf "${LSP_ARCHIVE}" -C "${LSP_EXTRACT_DIR}/"
    success "  Extracted to: ${LSP_EXTRACT_DIR}"
else
    warn "  Already extracted: ${LSP_EXTRACT_DIR}"
fi

# --- slint-viewer ------------------------------------------------------------
VIEWER_ARCHIVE="${EXTERNAL_DIR}/slint-viewer-linux.tar.gz"
VIEWER_EXTRACT_DIR="${EXTERNAL_DIR}/slint-viewer"

if [[ ! -d "${VIEWER_EXTRACT_DIR}" ]]; then
    info "  Extracting slint-viewer..."
    mkdir -p "${VIEWER_EXTRACT_DIR}"
    tar -xzf "${VIEWER_ARCHIVE}" -C "${VIEWER_EXTRACT_DIR}/"
    success "  Extracted to: ${VIEWER_EXTRACT_DIR}"
else
    warn "  Already extracted: ${VIEWER_EXTRACT_DIR}"
fi

# ---------------------------------------------------------------------------
# Step 4 – Install Slint C++ SDK system-wide
# ---------------------------------------------------------------------------
info "Installing Slint C++ SDK..."

if [[ ! -d "${CPP_EXTRACT_DIR}" ]]; then
    error "Slint C++ SDK extraction directory not found: ${CPP_EXTRACT_DIR}"
fi

# -- Headers: SDK include/ -> /usr/local/include/ ----------------------------
if [[ -d "${CPP_EXTRACT_DIR}/include" ]]; then
    info "  Headers   : ${CPP_EXTRACT_DIR}/include/ -> ${DEST_INC}/"
    cp -r "${CPP_EXTRACT_DIR}/include/." "${DEST_INC}/"
else
    warn "  No include/ directory found in SDK, skipping headers."
fi

# -- Libraries: SDK lib/ -> /usr/local/lib/ -----------------------------------
if [[ -d "${CPP_EXTRACT_DIR}/lib" ]]; then
    info "  Libraries : ${CPP_EXTRACT_DIR}/lib/ -> ${DEST_LIB}/"
    # Copy shared/static libs
    find "${CPP_EXTRACT_DIR}/lib" -maxdepth 1 \( -name "*.so*" -o -name "*.a" \) \
        -exec cp -P {} "${DEST_LIB}/" \;
    # Copy CMake configs
    if [[ -d "${CPP_EXTRACT_DIR}/lib/cmake" ]]; then
        info "  CMake cfg : ${CPP_EXTRACT_DIR}/lib/cmake/ -> ${DEST_CMAKE}/"
        mkdir -p "${DEST_CMAKE}"
        cp -r "${CPP_EXTRACT_DIR}/lib/cmake/." "${DEST_CMAKE}/"
    fi
else
    warn "  No lib/ directory found in SDK, skipping libraries."
fi

# -- SDK binaries: bin/ -> /usr/local/bin/ ------------------------------------
if [[ -d "${CPP_EXTRACT_DIR}/bin" ]]; then
    for sdk_bin in slint-compiler; do
        bin_path="$(find "${CPP_EXTRACT_DIR}/bin" -maxdepth 1 -type f -name "${sdk_bin}" | head -n1)"
        if [[ -n "${bin_path}" ]]; then
            info "  Binary    : ${sdk_bin} -> ${DEST_BIN}/"
            install -Dm755 "${bin_path}" "${DEST_BIN}/${sdk_bin}"
        else
            warn "  ${sdk_bin} not found in SDK bin/, skipping."
        fi
    done
fi

# Refresh the dynamic linker cache so libslint_cpp.so is found immediately
ldconfig
success "Slint C++ SDK installed."

# ---------------------------------------------------------------------------
# Step 5 – Install slint-lsp -> /usr/local/bin/slint-lsp
# ---------------------------------------------------------------------------
info "Installing slint-lsp -> ${DEST_BIN}/slint-lsp ..."

LSP_BIN="$(find "${LSP_EXTRACT_DIR}" -maxdepth 2 -type f -name "slint-lsp" | head -n1)"
if [[ -z "${LSP_BIN}" ]]; then
    error "Could not locate the slint-lsp binary inside ${LSP_EXTRACT_DIR}"
fi

install -Dm755 "${LSP_BIN}" "${DEST_BIN}/slint-lsp"
success "slint-lsp installed -> ${DEST_BIN}/slint-lsp"

# ---------------------------------------------------------------------------
# Step 6 – Install slint-viewer -> /usr/local/bin/slint-viewer
# ---------------------------------------------------------------------------
info "Installing slint-viewer -> ${DEST_BIN}/slint-viewer ..."

VIEWER_BIN="$(find "${VIEWER_EXTRACT_DIR}" -maxdepth 2 -type f -name "slint-viewer" | head -n1)"
if [[ -z "${VIEWER_BIN}" ]]; then
    error "Could not locate the slint-viewer binary inside ${VIEWER_EXTRACT_DIR}"
fi

install -Dm755 "${VIEWER_BIN}" "${DEST_BIN}/slint-viewer"
success "slint-viewer installed -> ${DEST_BIN}/slint-viewer"

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------
echo ""
echo -e "${GREEN}=========================================${NC}"
echo -e "${GREEN}  Slint v${SLINT_VERSION} installed successfully!${NC}"
echo -e "${GREEN}=========================================${NC}"
echo ""
echo "  slint-compiler : ${DEST_BIN}/slint-compiler"
echo "  slint-lsp      : ${DEST_BIN}/slint-lsp"
echo "  slint-viewer   : ${DEST_BIN}/slint-viewer"
echo "  Headers        : ${DEST_INC}/slint/"
echo "  Library        : ${DEST_LIB}/libslint_cpp.so"
echo "  CMake config   : ${DEST_CMAKE}/Slint/"
echo ""
info "Verify with: slint-compiler --version && slint-lsp --version && slint-viewer --version"
