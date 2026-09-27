#!/usr/bin/env bash
# Install the pinned runtime used to supervise bounded extraction
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/resolve_dep_base.sh"
source "$SCRIPT_DIR/verify_sha256.sh"

case "$(get_host_os)" in
windows)
    if command -v pwsh >/dev/null 2>&1; then
        exec pwsh -NoProfile -File "$SCRIPT_DIR/get_mac_oracle_tools.ps1"
    fi
    exec powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$SCRIPT_DIR/get_mac_oracle_tools.ps1"
    ;;
linux)
    if [ "$(uname -m)" != x86_64 ]; then
        echo 'ERROR: the pinned bounded Python package currently requires Linux x86-64' >&2
        exit 1
    fi
    ;;
*)
    echo 'ERROR: no pinned bounded Python package for this host' >&2
    exit 1
    ;;
esac

DEST="$LOCAL_DIR/$PYTHON_BOUNDED_LINUX_DIR_NAME"
begin_dependency_install "$DEST"
check_runtime() {
    local root="$1"
    [ -x "$root/python/bin/python3.12" ] || return 1
    pwsh -NoProfile -File "$SCRIPT_DIR/verify_dependency_tree.ps1" -Path "$root/python" \
        -ExpectedSha256 "$PYTHON_BOUNDED_LINUX_TREE_SHA256" || return 1
    "$root/python/bin/python3.12" -I -B -c 'import platform,sys; sys.exit(platform.python_version() != sys.argv[1])' "$PYTHON_BOUNDED_VERSION"
}
if [ -e "$DEST" ]; then
    check_runtime "$DEST"
    echo "Verified bounded Python $PYTHON_BOUNDED_VERSION already installed at $DEST"
    exit 0
fi
prepare_dependency_workspace "$DEST"
download_file "$DEPENDENCY_ARCHIVE" "$PYTHON_BOUNDED_LINUX_URL"
verify_sha256 "$DEPENDENCY_ARCHIVE" "$PYTHON_BOUNDED_LINUX_SHA256" 'bounded Python package'
tar -xzf "$DEPENDENCY_ARCHIVE" -C "$DEPENDENCY_STAGE_DIR"
check_runtime "$DEPENDENCY_STAGE_DIR"
publish_dependency_directory "$DEPENDENCY_STAGE_DIR" "$DEST"
echo "Installed verified bounded Python $PYTHON_BOUNDED_VERSION at $DEST"
