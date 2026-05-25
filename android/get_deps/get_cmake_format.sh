#!/bin/bash
# get_cmake_format.sh -- Install cmakelang (cmake-format + cmake-lint).
# Reads CMAKELANG_VERSION and PYTHON_EMBED_VERSION from tool_versions.conf.
# Source: https://github.com/cheshirekow/cmakelang (PyPI: cmakelang)
#
# Layout after install: $DEP_BASE/cmakelang-<ver>/
#   On Windows: bundles an embedded Python under python/ and installs
#               cmakelang into it, with cmake-format.exe / cmake-lint.exe
#               in python/Scripts/.
#   On Linux/macOS: creates a venv under venv/ using the host python3 when
#                   available, else a pinned virtualenv.pyz fallback, with
#                   cmake-format / cmake-lint in venv/bin/.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/tool_versions.conf"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"
DEST="$INSTALL_DIR/cmakelang-$CMAKELANG_VERSION"

_is_windows_target() {
    case "$DEST" in
    /mnt/[a-z]/*) return 0 ;;
    esac
    case "$(uname -s)" in
    MINGW* | MSYS* | CYGWIN* | *_NT*) return 0 ;;
    esac
    return 1
}

if _is_windows_target; then
    PY_EXE="$DEST/python/python.exe"
    CMAKE_FORMAT="$DEST/python/Scripts/cmake-format.exe"
else
    PY_EXE="$DEST/venv/bin/python"
    CMAKE_FORMAT="$DEST/venv/bin/cmake-format"
fi

if [ -f "$CMAKE_FORMAT" ]; then
    echo "cmakelang $CMAKELANG_VERSION already installed at $DEST"
    "$CMAKE_FORMAT" --version || true
    exit 0
fi

mkdir -p "$DEST"

if _is_windows_target; then
    # --- Windows: download embeddable Python distribution ---
    echo "Downloading Python $PYTHON_EMBED_VERSION embeddable..."
    echo "  URL: $PYTHON_EMBED_URL"
    PY_ZIP="$(mktemp -p "${TMPDIR:-/tmp}" python-embed-XXXXXX.zip)"
    download_file "$PY_ZIP" "$PYTHON_EMBED_URL"
    rm -rf "$DEST/python"
    mkdir -p "$DEST/python"
    unzip -q "$PY_ZIP" -d "$DEST/python"
    rm -f "$PY_ZIP"

    # Enable site-packages in the embeddable distribution. The shipped
    # python<minor>._pth disables site by default; we replace it.
    PY_MAJOR_MINOR_NODOT="$(echo "$PYTHON_EMBED_VERSION" | awk -F. '{printf "%s%s", $1, $2}')"
    PTH_FILE="$DEST/python/python${PY_MAJOR_MINOR_NODOT}._pth"
    if [ -f "$PTH_FILE" ]; then
        cat >"$PTH_FILE" <<EOF
python${PY_MAJOR_MINOR_NODOT}.zip
.
Lib
Lib/site-packages
import site
EOF
        mkdir -p "$DEST/python/Lib/site-packages"
    fi

    echo "Bootstrapping pip..."
    GET_PIP="$(mktemp -p "${TMPDIR:-/tmp}" get-pip-XXXXXX.py)"
    download_file "$GET_PIP" https://bootstrap.pypa.io/get-pip.py
    "$PY_EXE" "$GET_PIP" --no-warn-script-location --disable-pip-version-check
    rm -f "$GET_PIP"
else
    # --- Linux/macOS: use host python3 with a virtualenv ---
    if ! command -v python3 >/dev/null 2>&1; then
        echo "python3 not found on PATH; install Python 3.8+ and re-run"
        exit 1
    fi
    rm -rf "$DEST/venv"
    echo "Creating venv at $DEST/venv ..."
    if python3 -c 'import venv, ensurepip' >/dev/null 2>&1; then
        python3 -m venv "$DEST/venv"
    else
        VENV_PYZ="$(mktemp -p "${TMPDIR:-/tmp}" virtualenv-XXXXXX.pyz)"
        echo "python3 venv support missing, bootstrapping virtualenv $VIRTUALENV_VERSION..."
        download_file "$VENV_PYZ" "$VIRTUALENV_PYZ_URL"
        python3 "$VENV_PYZ" "$DEST/venv"
        rm -f "$VENV_PYZ"
    fi
fi

echo "Installing cmakelang $CMAKELANG_VERSION..."
"$PY_EXE" -m pip install --no-warn-script-location --disable-pip-version-check \
    "cmakelang==$CMAKELANG_VERSION" pyyaml

echo "cmakelang $CMAKELANG_VERSION installed at $DEST"
"$CMAKE_FORMAT" --version

if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
