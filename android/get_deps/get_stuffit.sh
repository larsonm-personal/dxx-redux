#!/bin/bash
# get_stuffit.sh - Build and install the stuffit CLI from Rust crate.
# Handles StuffIt (.sit) archives (SIT! 1.x and StuffIt 5.0 formats).
# Note: For StuffIt Installer v2 (STi2) archives, use unar instead.
# Requires: Rust toolchain (cargo) already installed.
# Reads version from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/tool_versions.conf"
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"
DEST="$INSTALL_DIR/$STUFFIT_DIR_NAME"

# Check for existing install with correct version
if [ -f "$DEST/bin/stuffit.exe" ] || [ -f "$DEST/bin/stuffit" ]; then
    INSTALLED_VER=$("$DEST/bin/stuffit" --version 2>/dev/null | grep -oP '\d+\.\d+\.\d+' || echo "")
    if [ "$INSTALLED_VER" = "$STUFFIT_CRATE_VERSION" ]; then
        echo "stuffit $STUFFIT_CRATE_VERSION already installed at $DEST"
        exit 0
    fi
fi

# Check for cargo
if ! command -v cargo >/dev/null 2>&1; then
    echo "ERROR: cargo not found. Install Rust from https://rustup.rs/ first"
    exit 1
fi

echo "Building stuffit $STUFFIT_CRATE_VERSION from crates.io..."
cargo install stuffit --version "$STUFFIT_CRATE_VERSION" --root "$DEST"

if [ -f "$DEST/bin/stuffit.exe" ] || [ -f "$DEST/bin/stuffit" ]; then
    echo "stuffit $STUFFIT_CRATE_VERSION installed at $DEST"
else
    echo "ERROR: stuffit binary not found after build"
    exit 1
fi

if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
