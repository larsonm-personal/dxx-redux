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
source "$SCRIPT_DIR/../tool_versions.conf"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"
DEST="$INSTALL_DIR/cmakelang-$CMAKELANG_VERSION"
begin_dependency_install "$DEST"

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

check_cmakelang_install() {
    [ -f "$CMAKE_FORMAT" ] && [ -f "$PY_EXE" ] || return 1
    "$PY_EXE" -c 'import sys; from importlib.metadata import version; sys.exit(0 if [version(p) for p in ("cmakelang", "PyYAML", "six")] == sys.argv[1:] else 1)' \
        "$CMAKELANG_VERSION" "$CMAKELANG_PYYAML_VERSION" "$CMAKELANG_SIX_VERSION" || return 1
    "$CMAKE_FORMAT" --version || return 1
    "$PY_EXE" -m cmakelang.lint --version
}

if check_cmakelang_install >/dev/null 2>&1; then
    echo "cmakelang $CMAKELANG_VERSION already installed at $DEST"
    "$CMAKE_FORMAT" --version
    exit 0
fi

# Python console launchers embed the final interpreter path, so install in place
# Preserve the previous tree until validation and roll back ordinary failures
if [ -n "${DEPENDENCY_INSTALL_STATE:-}" ]; then
    prepare_in_place_dependency_install "$DEST"
    WORK_DIR="$DEPENDENCY_WORK_DIR"
else
    assert_dependency_disk_space "$INSTALL_DIR" 1
    if [ -L "$DEST" ]; then
        echo "ERROR: refusing linked installation path: $DEST" >&2
        exit 1
    fi
    WORK_DIR="$(create_temp_dir .dxx-cmakelang-stage "$INSTALL_DIR")"
    INSTALL_STARTED=0
    INSTALL_VALIDATED=0
    cleanup_cmakelang_install() {
        local status=$?
        if [ "$INSTALL_VALIDATED" != 1 ]; then
            if [ -d "$WORK_DIR/previous" ]; then
                rm -rf "$DEST"
                if ! mv "$WORK_DIR/previous" "$DEST"; then
                    echo "ERROR: restore $WORK_DIR/previous to $DEST before retrying" >&2
                    return 1
                fi
            elif [ "$INSTALL_STARTED" = 1 ]; then
                rm -rf "$DEST"
            fi
        fi
        rm -rf "$WORK_DIR"
        return "$status"
    }
    trap cleanup_cmakelang_install EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM
    if [ -e "$DEST" ]; then mv "$DEST" "$WORK_DIR/previous"; fi
    INSTALL_STARTED=1
    mkdir -p "$DEST"
fi

if _is_windows_target; then
    # --- Windows: download embeddable Python distribution ---
    echo "Downloading Python $PYTHON_EMBED_VERSION embeddable..."
    echo "  URL: $PYTHON_EMBED_URL"
    PY_ZIP="$WORK_DIR/python-embed.zip"
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
    GET_PIP="$WORK_DIR/get-pip.py"
    download_file "$GET_PIP" https://bootstrap.pypa.io/get-pip.py
    "$PY_EXE" "$GET_PIP" --no-cache-dir --no-warn-script-location --disable-pip-version-check
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
        VENV_PYZ="$WORK_DIR/virtualenv.pyz"
        echo "python3 venv support missing, bootstrapping virtualenv $VIRTUALENV_VERSION..."
        download_file "$VENV_PYZ" "$VIRTUALENV_PYZ_URL"
        python3 "$VENV_PYZ" "$DEST/venv"
        rm -f "$VENV_PYZ"
    fi
fi

echo "Installing cmakelang $CMAKELANG_VERSION..."
"$PY_EXE" -m pip install --no-cache-dir --no-warn-script-location --disable-pip-version-check \
    --no-deps "cmakelang==$CMAKELANG_VERSION" "pyyaml==$CMAKELANG_PYYAML_VERSION" "six==$CMAKELANG_SIX_VERSION"

check_cmakelang_install
INSTALL_VALIDATED=1
if [ -n "${DEPENDENCY_INSTALL_STATE:-}" ]; then commit_in_place_dependency_install; fi
echo "cmakelang $CMAKELANG_VERSION installed at $DEST"

if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
